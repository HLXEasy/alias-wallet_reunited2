// Copyright (c) 2016-2019 The Spectrecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BOOTSTRAPWIZARD_H
#define BOOTSTRAPWIZARD_H

#include <QWizard>

QT_BEGIN_NAMESPACE
class QLabel;
class QRadioButton;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

class BootstrapWizard : public QWizard
{
    Q_OBJECT

public:
    enum { Page_Intro,
           Page_Download,
           Page_Sync };

    BootstrapWizard(QWidget *parent = 0);

    void showSideWidget();

protected:
    void showEvent(QShowEvent *);

private slots:
    void showHelp();
    void pageChanged(int id);

private:

};

class BootstrapIntroPage : public QWizardPage
{
    Q_OBJECT

public:
    BootstrapIntroPage(QWidget *parent = 0);

    int nextId() const override;

private:
    QLabel *topLabel;
    QRadioButton *rbBoostrap;
    QRadioButton *rbSync;
};

class DownloadPage : public QWizardPage
{
    Q_OBJECT

public:
    DownloadPage(QWidget *parent = 0);

    int nextId() const override;
    void initializePage() override;
    bool isComplete() const override;

public slots:
    void startDownload();
    void updateBootstrapState(int state, int progress, bool indeterminate);

private:
   QLabel *progressLabel;
   QProgressBar *progressBar;
   QPushButton *downloadButton;

   int state;
   int progress;
   bool indeterminate;
};

class BootstrapSyncPage : public QWizardPage
{
    Q_OBJECT

public:
    BootstrapSyncPage(QWidget *parent = 0);

    int nextId() const override;

private:
    QLabel *noteLabel;
};


#endif // BOOTSTRAPWIZARD_H
