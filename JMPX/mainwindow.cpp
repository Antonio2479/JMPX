#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QDebug>
#include <QStatusBar>

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(width(),height());

    ui->scavolumemeter->setDirection(Volumemeter::Vertical);
    ui->lvolumemeter->setDirection(Volumemeter::Vertical);
    ui->rvolumemeter->setDirection(Volumemeter::Vertical);
    ui->outvolumemeter->setDirection(Volumemeter::Vertical);
    QTimer::singleShot(0,this,SLOT(volbarssetfixedequalwidth()));//for some reason the two volume bars a normally one pixel different this sets them the same

    //create options dialog.
    options = new Options(this);
    connect(options,SIGNAL(Show_SCA_Volume_Meter_signal(bool)),this,SLOT(Show_SCA_Volume_Meter(bool)));
    if(QCoreApplication::arguments().last().toUpper()=="QUIT_ON_ERROR")options->quit_on_error=true;

    //load library
    QLibrary library;
    if (!library.load())library.setFileName("../build-libJMPX-Desktop_64bit_MinGW-Release/release/libJMPX");//for me
    if (!library.load())library.setFileName("../build-libJMPX-Desktop_Qt_5_5_0_MinGW_32bit-Release/release/libJMPX");//me too
    if (!library.load())library.setFileName(QApplication::applicationDirPath()+"/liblibJMPX");
    if (!library.load())library.setFileName(QApplication::applicationDirPath()+"/libJMPX");
    if (!library.load())library.setFileName("libJMPX");
    if (!library.load())
    {
        qDebug() << library.errorString();
        if(options->quit_on_error)exit(1);
        QMessageBox::critical(this,"Error","<p><b>Error loading JMPX library.</b></p><p>"+library.errorString()+"</p><p>Note: The JMPX library needs to be placed in the gui's program directory or in system path. On Windows this should be called libJMPX.dll on linux something like libJQAM.so. Please find it and copy it over for JMPX to work.</p>");
    }
    if (library.load())
    {
        //qDebug() << "library loaded";
        createJMPXfunction createJMPX = (createJMPXfunction)library.resolve("createObject");
        pJMPX = createJMPX(this);
        if(pJMPX)
        {
            pJMPX->SetSampleRate(192000);
        }
         else
         {
            if(options->quit_on_error){qDebug("Error loading JMPX library: Failed to create device");exit(1);}
            QMessageBox::critical(this,"Error","<p><b>Error loading JMPX library.</b></p><p>Failed to create device</p>");
         }
    }

    //song title checker (has no settings)
    nowplaying=new NowPlaying(this);
    connect(nowplaying,SIGNAL(songtitlechanged(QString)),this,SLOT(songtitlecheck(QString)));

    //Fileloader for group 5A (has settings)
    fileloader=new FileLoader(this);
    connect(fileloader,SIGNAL(dataLoadSignal(QByteArray)),this,SLOT(pushdatato5a(QByteArray)));

    //load settings
    if(pJMPX)options->loadsettings(pJMPX,fileloader);

    //update low rate info
    updatelowrateinfo();

    //display update init
    ptimer= new QTimer(this);
    ptimer->setInterval(20);
    connect(ptimer,SIGNAL(timeout()),this,SLOT(updatedisplay()));

    //restore modulate enable
    QSettings settings("JontiSoft", "JMPX");
    ui->checkBox_modulate->setChecked(settings.value("checkBox_modulate",false).toBool());

}

MainWindow::~MainWindow()
{
    //save settings
    if(pJMPX)options->savesettings(pJMPX,fileloader);
    QSettings settings("JontiSoft", "JMPX");
    settings.setValue("checkBox_modulate",ui->checkBox_modulate->isChecked());
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::updatedisplay()
{
    if(!pJMPX)return;
    TSigStats *psigstats=pJMPX->GetSignalStats();
    ui->scavolumemeter->setVolume(psigstats->scavol);
    ui->lvolumemeter->setVolume(psigstats->lvol);
    ui->rvolumemeter->setVolume(psigstats->rvol);
    ui->outvolumemeter->setVolume(psigstats->outvol);
    if(pJMPX->GetSCAopus()&&ui->groupBox_sca->isVisible())statusBar()->showMessage(((QString)"Opus buffer usage: %1%").arg(qRound(100.0*psigstats->opusbufferuseagepercent)),1000);

}

void MainWindow::on_action_Options_triggered()
{
    if(!pJMPX)return;
    options->populatesettings(pJMPX,fileloader);
    if(options->exec()==QDialog::Accepted)
    {
        options->pushsetting(pJMPX,fileloader);

        ui->checkBox_modulate->setChecked(pJMPX->IsActive());
        songtitlecheck(nowplaying->rt_title);
    }
    updatelowrateinfo();
}

void MainWindow::Show_SCA_Volume_Meter(bool show)
{
    if(ui->groupBox_sca->isHidden()!=show)return;
    int xshift=ui->groupBox_sca->pos().x()-ui->groupBox_in->pos().x();
    if(show)xshift=-xshift;
    ui->groupBox_out->move(ui->groupBox_out->pos().x()-xshift,ui->groupBox_out->pos().y());
    ui->widget_therest->move(ui->widget_therest->pos().x()-xshift,ui->widget_therest->pos().y());
    setFixedSize(width()-xshift,height());
    ui->groupBox_sca->setVisible(show);
}

void MainWindow::volbarssetfixedequalwidth()
{
    if(ui->lvolumemeter->width()!=ui->rvolumemeter->width())
    {
        int width=max(ui->lvolumemeter->width(),ui->rvolumemeter->width());
        ui->lvolumemeter->setFixedWidth(width);
        ui->rvolumemeter->setFixedWidth(width);
    }
}

void MainWindow::updatelowrateinfo()
{
    if(!pJMPX)return;
    if(pJMPX->GetEnableStereo())ui->label_stereo->setPixmap(QPixmap(":/images/stereo-on.png"));
     else ui->label_stereo->setPixmap(QPixmap(":/images/stereo-off.png"));
    if(pJMPX->GetEnableRDS())ui->label_rds->setPixmap(QPixmap(":/images/RDS-on.png"));
     else ui->label_rds->setPixmap(QPixmap(":/images/RDS-off.png"));
}

void MainWindow::on_checkBox_modulate_stateChanged(int state)
{
    if(!pJMPX)return;
    if((!pJMPX->IsActive())&&(state))
    {
        pJMPX->SetSampleRate(192000);
    }
    pJMPX->Active(state);
    if((state)&&(pJMPX->GotError()))
    {
        QMessageBox msgBox;
        if(options->quit_on_error){qDebug()<<pJMPX->GetLastRTAudioError();exit(1);}
        msgBox.setText(pJMPX->GetLastRTAudioError());
        msgBox.exec();
        state=false;
        pJMPX->Active(false);
    }
    if(state)ptimer->start();
     else ptimer->stop();
    ui->scavolumemeter->setVolume(0);
    ui->lvolumemeter->setVolume(0);
    ui->rvolumemeter->setVolume(0);
    ui->outvolumemeter->setVolume(0);
    statusBar()->clearMessage();

    if(state)songtitlecheck(nowplaying->rt_title);

}

void MainWindow::on_action_About_triggered()
{
    QMessageBox::about(this,"JMPX",""
                                     "<H1>Stereo and RDS encoder for FM transmitters</H1>"
                                     "<H3>v2.1.0</H3>"
                                     "<p>When connected to an FM transmitter via a soundcard this program allows you to transmit in stereo along with the ability to send information using RDS (Radio Data System) to the listeners. With RDS the listeners can see what your station is called and other useful information. SCA allows another mono sub carrier to be added to the signal that can be received with a special receiver. DSCA is a digital version of SCA and also requires a special receiver.</p>"
                                     "<p>For more information about this application see <a href=\"http://jontio.zapto.org/hda1/paradise/jmpxencoder/jmpx.html\">http://jontio.zapto.org/hda1/paradise/jmpxencoder/jmpx.html</a>.</p>"
                                     "<p>Jonti 2017</p>" );
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::songtitlecheck(const QString &title)
{
    if((!pJMPX)||!pJMPX.data()->IsActive())return;

    //does the user want us to update rt with title?
    if(!options->update_rt_music_title)
    {
        pJMPX->RDS_SetRT("");
        return;
    }

    if(title!=pJMPX->RDS_GetRT())qDebug()<<"Updated RT to song title: "<<title;

    //set RT with title
    pJMPX->RDS_SetRT(title);
}

void MainWindow::pushdatato5a(const QByteArray &data)
{
    if(!pJMPX)return;
    pJMPX->RDS_Set_5A_data(data);
}
