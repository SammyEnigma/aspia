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

#include "client/ui/export_password_dialog.h"

#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "common/ui/msg_box.h"

//--------------------------------------------------------------------------------------------------
ExportPasswordDialog::ExportPasswordDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ExportPasswordDialog::onShowPasswordButtonToggled);
    connect(ui.button_encrypt, &QPushButton::clicked,
            this, &ExportPasswordDialog::onEncryptClicked);
    connect(ui.button_skip, &QPushButton::clicked,
            this, &ExportPasswordDialog::onSkipClicked);
    connect(ui.button_cancel, &QPushButton::clicked, this, &QDialog::reject);

    ui.edit_password->setFocus();

    QTimer::singleShot(0, this, [this](){ setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
ExportPasswordDialog::~ExportPasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QString ExportPasswordDialog::password() const
{
    return ui.edit_password->text();
}

//--------------------------------------------------------------------------------------------------
void ExportPasswordDialog::onShowPasswordButtonToggled(bool checked)
{
    QLineEdit::EchoMode mode = checked ? QLineEdit::Normal : QLineEdit::Password;
    ui.edit_password->setEchoMode(mode);
    ui.edit_confirm->setEchoMode(mode);

    if (checked)
    {
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
        ui.edit_confirm->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        const Qt::InputMethodHints hints = Qt::ImhHiddenText | Qt::ImhSensitiveData |
            Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;
        ui.edit_password->setInputMethodHints(hints);
        ui.edit_confirm->setInputMethodHints(hints);
    }
}

//--------------------------------------------------------------------------------------------------
void ExportPasswordDialog::onEncryptClicked()
{
    QString password = ui.edit_password->text();
    if (password.isEmpty())
    {
        MsgBox::warning(this, tr("Password cannot be empty."));
        return;
    }

    if (password != ui.edit_confirm->text())
    {
        MsgBox::warning(this, tr("Passwords do not match."));
        return;
    }

    result_ = Result::ENCRYPT;
    accept();
}

//--------------------------------------------------------------------------------------------------
void ExportPasswordDialog::onSkipClicked()
{
    int answer = MsgBox::question(this,
        tr("Without a password, user names and passwords will not be exported. Continue?"),
        MsgBox::Yes | MsgBox::No);
    if (answer != MsgBox::Yes)
        return;

    result_ = Result::SKIP;
    accept();
}
