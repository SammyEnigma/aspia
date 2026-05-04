//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/settings_dialog.h"

#include <QAbstractButton>
#include <QFileDialog>
#include <QPushButton>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "common/ui/msg_box.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/ui/application.h"
#include "client/ui/master_password_dialog.h"
#include "common/ui/update_dialog.h"

//--------------------------------------------------------------------------------------------------
SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    Settings settings;

    // General tab.
    QString current_locale = settings.locale();
    Application::LocaleList locale_list = GuiApplication::instance()->localeList();
    for (const auto& locale : std::as_const(locale_list))
    {
        ui.combo_language->addItem(locale.second, locale.first);
        if (locale.first == current_locale)
            ui.combo_language->setCurrentIndex(ui.combo_language->count() - 1);
    }

    QString current_theme = settings.theme();
    QStringList available_themes = GuiApplication::instance()->availableThemes();
    for (const QString& theme_id : std::as_const(available_themes))
    {
        ui.combo_theme->addItem(GuiApplication::themeName(theme_id), theme_id);
        if (theme_id == current_theme)
            ui.combo_theme->setCurrentIndex(ui.combo_theme->count() - 1);
    }

    Database& db = Database::instance();
    ui.edit_display_name->setText(db.displayName());

    // Master Password.
    connect(ui.button_change_master_password, &QPushButton::clicked,
            this, &SettingsDialog::onChangeMasterPassword);

    // Desktop tab.
    proto::control::Config desktop_config = settings.desktopConfig();
    ui.checkbox_audio->setChecked(desktop_config.audio());
    ui.checkbox_clipboard->setChecked(desktop_config.clipboard());
    ui.checkbox_cursor_shape->setChecked(desktop_config.cursor_shape());
    ui.checkbox_enable_cursor_pos->setChecked(desktop_config.cursor_position());
    ui.checkbox_desktop_effects->setChecked(!desktop_config.effects());
    ui.checkbox_desktop_wallpaper->setChecked(!desktop_config.wallpaper());
    ui.checkbox_lock_at_disconnect->setChecked(desktop_config.lock_at_disconnect());
    ui.checkbox_block_remote_input->setChecked(desktop_config.block_input());
    ui.checkbox_send_key_combinations->setChecked(settings.sendKeyCombinations());

    ui.checkbox_record_autostart->setChecked(settings.recordSessions());
    ui.edit_record_dir->setText(settings.recordingPath());
    connect(ui.button_select_record_dir, &QPushButton::clicked,
            this, &SettingsDialog::onSelectRecordingPath);

    // Update tab.
#if defined(Q_OS_WINDOWS)
    ui.checkbox_check_updates->setChecked(db.isCheckUpdatesEnabled());

    QString update_server = db.updateServer();
    ui.edit_update_server->setText(update_server);

    if (update_server == DEFAULT_UPDATE_SERVER)
    {
        ui.checkbox_custom_server->setChecked(false);
        ui.edit_update_server->setEnabled(false);
    }
    else
    {
        ui.checkbox_custom_server->setChecked(true);
        ui.edit_update_server->setEnabled(true);
    }

    connect(ui.checkbox_custom_server, &QCheckBox::toggled, this, [this](bool checked)
    {
        LOG(INFO) << "[ACTION] Custom server checkbox:" << checked;
        ui.edit_update_server->setEnabled(checked);

        if (!checked)
            ui.edit_update_server->setText(DEFAULT_UPDATE_SERVER);
    });

    connect(ui.button_check_for_updates, &QPushButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] Check for updates";
        UpdateDialog(ui.edit_update_server->text(), "client", this).exec();
    });
#else
    ui.tabbar->setTabVisible(ui.tabbar->indexOf(ui.tab_update), false);
#endif

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &SettingsDialog::onButtonBoxClicked);
}

//--------------------------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";
    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }
    else
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        Settings settings;

        // Save language.
        QString new_locale = ui.combo_language->currentData().toString();
        settings.setLocale(new_locale);
        Application::instance()->setLocale(new_locale);

        // Save theme.
        QString new_theme = ui.combo_theme->currentData().toString();
        settings.setTheme(new_theme);
        GuiApplication::instance()->setTheme(new_theme);

        proto::control::Config desktop_config;
        desktop_config.set_audio(ui.checkbox_audio->isChecked());
        desktop_config.set_clipboard(ui.checkbox_clipboard->isChecked());
        desktop_config.set_cursor_shape(ui.checkbox_cursor_shape->isChecked());
        desktop_config.set_cursor_position(ui.checkbox_enable_cursor_pos->isChecked());
        desktop_config.set_effects(!ui.checkbox_desktop_effects->isChecked());
        desktop_config.set_wallpaper(!ui.checkbox_desktop_wallpaper->isChecked());
        desktop_config.set_lock_at_disconnect(ui.checkbox_lock_at_disconnect->isChecked());
        desktop_config.set_block_input(ui.checkbox_block_remote_input->isChecked());
        settings.setDesktopConfig(desktop_config);

        settings.setRecordSessions(ui.checkbox_record_autostart->isChecked());
        settings.setRecordingPath(ui.edit_record_dir->text());
        settings.setSendKeyCombinations(ui.checkbox_send_key_combinations->isChecked());

        Database& db = Database::instance();
        db.setDisplayName(ui.edit_display_name->text());

#if defined(Q_OS_WINDOWS)
        db.setCheckUpdatesEnabled(ui.checkbox_check_updates->isChecked());
        db.setUpdateServer(ui.edit_update_server->text().toLower());
#endif

        accept();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::showError(const QString& message)
{
    MsgBox::warning(this, message);
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onChangeMasterPassword()
{
    LOG(INFO) << "[ACTION] Change master password";

    MasterPasswordDialog dialog(MasterPasswordDialog::Mode::CHANGE, this);
    dialog.exec();
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onSelectRecordingPath()
{
    LOG(INFO) << "[ACTION] Select recording path";

    QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose path"), ui.edit_record_dir->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Recording path selection rejected";
        return;
    }

    LOG(INFO) << "[ACTION] Recording path selected:" << path;
    ui.edit_record_dir->setText(path);
}
