// Copyright (c) 2019 The Xep Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XEP_QT_CREATEWALLETDIALOG_H
#define XEP_QT_CREATEWALLETDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
    class CreateWalletDialog;
}

/** Dialog for creating wallets
 */
class CreateWalletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateWalletDialog(QWidget* parent);
    virtual ~CreateWalletDialog();

    QString walletName() const;
    bool isEncryptWalletChecked() const;
    bool isDisablePrivateKeysChecked() const;
    bool isMakeBlankWalletChecked() const;

private:
    Ui::CreateWalletDialog *ui;
};

#endif // XEP_QT_CREATEWALLETDIALOG_H
