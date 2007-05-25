/*****************************************************************************
 * open.cpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#include "qt4.hpp"
#include "components/open.hpp"
#include "dialogs/open.hpp"
#include "dialogs_provider.hpp"
#include "util/customwidgets.hpp"

#include <QFileDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QStackedLayout>
#include <QListView>

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

/**************************************************************************
 * File open
 **************************************************************************/
FileOpenPanel::FileOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    /* Classic UI Setup */
    ui.setupUi( this );

    /* Use a QFileDialog and customize it because we don't want to
       rewrite it all. Be careful to your eyes cause there are a few hacks.
       Be very careful and test correctly when you modify this. */

    /* Set Filters for file selection */
    QString fileTypes = "";
    ADD_FILTER_MEDIA( fileTypes );
    ADD_FILTER_VIDEO( fileTypes );
    ADD_FILTER_AUDIO( fileTypes );
    ADD_FILTER_PLAYLIST( fileTypes );
    ADD_FILTER_ALL( fileTypes );
    fileTypes.replace( QString(";*"), QString(" *"));

    // Make this QFileDialog a child of tempWidget from the ui.
    dialogBox = new FileOpenBox( ui.tempWidget, NULL,
            qfu( p_intf->p_libvlc->psz_homedir ), fileTypes );
/*    dialogBox->setFileMode( QFileDialog::ExistingFiles );*/
    dialogBox->setAcceptMode( QFileDialog::AcceptOpen );

    /* retrieve last known path used in file browsing */
    char *psz_filepath = config_GetPsz( p_intf, "qt-filedialog-path" );
    if( psz_filepath )
    {
        dialogBox->setDirectory( QString::fromUtf8( psz_filepath ) );
        delete psz_filepath;
    }

    /* We don't want to see a grip in the middle of the window, do we? */
    dialogBox->setSizeGripEnabled( false );
    dialogBox->setToolTip( qtr( "Select one or multiple files, or a folder" ));

    // Add it to the layout
    ui.gridLayout->addWidget( dialogBox, 0, 0, 1, 3 );

    // But hide the two OK/Cancel buttons. Enable them for debug.
    QDialogButtonBox *fileDialogAcceptBox =
                        findChildren<QDialogButtonBox*>()[0];
    fileDialogAcceptBox->hide();

    /* Ugly hacks to get the good Widget */
    //This lineEdit is the normal line in the fileDialog.
    lineFileEdit = findChildren<QLineEdit*>()[3];
    lineFileEdit->hide();

    /* Make a list of QLabel inside the QFileDialog to access the good ones */
    QList<QLabel *> listLabel = findChildren<QLabel*>();

    /* Hide the FileNames one. Enable it for debug */
    listLabel[4]->hide();
    /* Change the text that was uncool in the usual box */
    listLabel[5]->setText( qtr( "Filter:" ) );

#if WIN32
    /* QFileDialog is quite buggy make it brerable on win32 by tweaking 
       the followin */
    QListView *fileListView = findChildren<QListView*>().first();
    fileListView->setLayoutMode(QListView::Batched);
    fileListView->setViewMode(QListView::ListMode);
    fileListView->setResizeMode(QListView::Adjust);
    fileListView->setUniformItemSizes(false);
    fileListView->setFlow(QListView::TopToBottom);
    fileListView->setWrapping(true);
#endif

    // Hide the subtitles control by default.
    ui.subFrame->hide();

    /* Build the subs size combo box */
    module_config_t *p_item =
        config_FindConfig( VLC_OBJECT(p_intf), "freetype-rel-fontsize" );
    if( p_item )
    {
        for( int i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            ui.sizeSubComboBox->addItem(
                qfu( p_item->ppsz_list_text[i_index] ),
                QVariant( p_item->pi_list[i_index] ) );
            if( p_item->value.i == p_item->pi_list[i_index] )
            {
                ui.sizeSubComboBox->setCurrentIndex( i_index );
            }
        }
    }

    /* Build the subs align combo box */
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "subsdec-align" );
    if( p_item )
    {
        for( int i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            ui.alignSubComboBox->addItem(
                qfu( p_item->ppsz_list_text[i_index] ),
                QVariant( p_item->pi_list[i_index] ) );
            if( p_item->value.i == p_item->pi_list[i_index] )
            {
                ui.alignSubComboBox->setCurrentIndex( i_index );
            }
        }
    }

    /* Connects  */
    BUTTONACT( ui.subBrowseButton, browseFileSub() );
    BUTTONACT( ui.subCheckBox, toggleSubtitleFrame());

    CONNECT( ui.fileInput, editTextChanged( QString ), this, updateMRL());
    CONNECT( ui.subInput, editTextChanged( QString ), this, updateMRL());
    CONNECT( ui.alignSubComboBox, currentIndexChanged( int ), this, updateMRL());
    CONNECT( ui.sizeSubComboBox, currentIndexChanged( int ), this, updateMRL());
    CONNECT( lineFileEdit, textChanged( QString ), this, browseFile());
}

FileOpenPanel::~FileOpenPanel()
{}

QStringList FileOpenPanel::browse( QString help )
{
    return THEDP->showSimpleOpen( help );
}

/* Unused. FIXME ? */
void FileOpenPanel::browseFile()
{
    QString fileString = "";
    foreach( QString file, dialogBox->selectedFiles() ) {
         fileString += "\"" + file + "\" ";
    }
    ui.fileInput->setEditText( fileString );
    updateMRL();
}

void FileOpenPanel::browseFileSub()
{
    // FIXME Handle selection of more than one subtitles file
    QStringList files = THEDP->showSimpleOpen( qtr("Open subtitles file"),
                            EXT_FILTER_SUBTITLE,
                            dialogBox->directory().absolutePath() );
    if( files.isEmpty() ) return;
    ui.subInput->setEditText( files.join(" ") );
    updateMRL();
}

void FileOpenPanel::updateMRL()
{
    QString mrl = ui.fileInput->currentText();

    if( ui.subCheckBox->isChecked() ) {
        mrl.append( " :sub-file=" + ui.subInput->currentText() );
        int align = ui.alignSubComboBox->itemData(
                    ui.alignSubComboBox->currentIndex() ).toInt();
        mrl.append( " :subsdec-align=" + QString().setNum( align ) );
        int size = ui.sizeSubComboBox->itemData(
                   ui.sizeSubComboBox->currentIndex() ).toInt();
        mrl.append( " :freetype-rel-fontsize=" + QString().setNum( size ) );
    }

    const char *psz_filepath = config_GetPsz( p_intf, "qt-filedialog-path" );
    if( ( NULL == psz_filepath )
      || strcmp( psz_filepath,dialogBox->directory().absolutePath().toUtf8()) )
    {
        /* set dialog box current directory as last known path */
        config_PutPsz( p_intf, "qt-filedialog-path",
                       dialogBox->directory().absolutePath().toUtf8() );
    }
    delete psz_filepath;

    emit mrlUpdated( mrl );
    emit methodChanged( "file-caching" );
}


/* Function called by Open Dialog when clicke on Play/Enqueue */
void FileOpenPanel::accept()
{
    ui.fileInput->addItem( ui.fileInput->currentText());
    if ( ui.fileInput->count() > 8 ) ui.fileInput->removeItem( 0 );
}

void FileOpenBox::accept()
{
    OpenDialog::getInstance( NULL, NULL )->play();
}

/* Function called by Open Dialog when clicked on cancel */
void FileOpenPanel::clear()
{
    ui.fileInput->setEditText( "" );
    ui.subInput->setEditText( "" );
}

void FileOpenPanel::toggleSubtitleFrame()
{
    if ( ui.subFrame->isVisible() )
    {
        ui.subFrame->hide();
        updateGeometry();
    /* FiXME Size */
    }
    else
    {
        ui.subFrame->show();
    }

    /* Update the MRL */
    updateMRL();
}

/**************************************************************************
 * Disk open
 **************************************************************************/
DiscOpenPanel::DiscOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    BUTTONACT( ui.dvdRadioButton, updateButtons());
    BUTTONACT( ui.vcdRadioButton, updateButtons());
    BUTTONACT( ui.audioCDRadioButton, updateButtons());
    BUTTONACT( ui.dvdsimple,  updateButtons());

    CONNECT( ui.deviceCombo, editTextChanged( QString ), this, updateMRL());
    CONNECT( ui.titleSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.chapterSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.audioSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.subtitlesSpin, valueChanged( int ), this, updateMRL());
}

DiscOpenPanel::~DiscOpenPanel()
{}

void DiscOpenPanel::clear()
{
    ui.titleSpin->setValue( 0 );
    ui.chapterSpin->setValue( 0 );
}

void DiscOpenPanel::updateButtons()
{
    if ( ui.dvdRadioButton->isChecked() )
    {
        ui.titleLabel->setText( qtr("Title") );
        ui.chapterLabel->show();
        ui.chapterSpin->show();
        ui.diskOptionBox_2->show();
    }
    else if ( ui.vcdRadioButton->isChecked() )
    {
        ui.titleLabel->setText( qtr("Entry") );
        ui.chapterLabel->hide();
        ui.chapterSpin->hide();
        ui.diskOptionBox_2->show();
    }
    else
    {
        ui.titleLabel->setText( qtr("Track") );
        ui.chapterLabel->hide();
        ui.chapterSpin->hide();
        ui.diskOptionBox_2->hide();
    }

    updateMRL();
}


void DiscOpenPanel::updateMRL()
{
    QString mrl = "";
    /* DVD */
    if( ui.dvdRadioButton->isChecked() ) {
        if( !ui.dvdsimple->isChecked() )
            mrl = "dvd://";
        else
            mrl = "dvdsimple://";
        mrl += ui.deviceCombo->currentText();
        emit methodChanged( "dvdnav-caching" );

        if ( ui.titleSpin->value() > 0 ) {
            mrl += QString("@%1").arg( ui.titleSpin->value() );
            if ( ui.chapterSpin->value() > 0 ) {
                mrl+= QString(":%1").arg( ui.chapterSpin->value() );
            }
        }

    /* VCD */
    } else if ( ui.vcdRadioButton->isChecked() ) {
        mrl = "vcd://" + ui.deviceCombo->currentText();
        emit methodChanged( "vcd-caching" );

        if( ui.titleSpin->value() > 0 ) {
            mrl += QString("@E%1").arg( ui.titleSpin->value() );
        }

    /* CDDA */
    } else {
        mrl = "cdda://" + ui.deviceCombo->currentText();
        if( ui.titleSpin->value() > 0 ) {
            QString("@%1").arg( ui.titleSpin->value() );
        }
    }

    if ( ui.dvdRadioButton->isChecked() || ui.vcdRadioButton->isChecked() )
    {
        if ( ui.audioSpin->value() >= 0 ) {
            mrl += " :audio-track=" +
                QString("%1").arg( ui.audioSpin->value() );
        }
        if ( ui.subtitlesSpin->value() >= 0 ) {
            mrl += " :sub-track=" +
                QString("%1").arg( ui.subtitlesSpin->value() );
        }
    }

    emit mrlUpdated( mrl );
}



/**************************************************************************
 * Net open
 **************************************************************************/
NetOpenPanel::NetOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    CONNECT( ui.protocolCombo, currentIndexChanged( int ),
             this, updateProtocol( int ) );
    CONNECT( ui.portSpin, valueChanged( int ), this, updateMRL() );
    CONNECT( ui.addressText, textChanged( QString ), this, updateAddress());
    CONNECT( ui.timeShift, clicked(), this, updateMRL());
    CONNECT( ui.ipv6, clicked(), this, updateMRL());

    ui.protocolCombo->addItem("HTTP", QVariant("http"));
    ui.protocolCombo->addItem("HTTPS", QVariant("https"));
    ui.protocolCombo->addItem("FTP", QVariant("ftp"));
    ui.protocolCombo->addItem("MMS", QVariant("mms"));
    ui.protocolCombo->addItem("RTSP", QVariant("rtsp"));
    ui.protocolCombo->addItem("UDP/RTP (unicast)", QVariant("udp"));
    ui.protocolCombo->addItem("UDP/RTP (multicast)", QVariant("udp"));
}

NetOpenPanel::~NetOpenPanel()
{}

void NetOpenPanel::clear()
{}

void NetOpenPanel::updateProtocol( int idx ) {
    QString addr = ui.addressText->text();
    QString proto = ui.protocolCombo->itemData( idx ).toString();

    ui.timeShift->setEnabled( idx >= 4 );
    ui.ipv6->setEnabled( idx == 4 );
    ui.addressText->setEnabled( idx != 4 );
    ui.portSpin->setEnabled( idx >= 4 );

    /* If we already have a protocol in the address, replace it */
    if( addr.contains( "://")) {
        msg_Err( p_intf, "replace");
        addr.replace( QRegExp("^.*://"), proto + "://");
        ui.addressText->setText( addr );
    }

    updateMRL();
}

void NetOpenPanel::updateAddress() {
    updateMRL();
}

void NetOpenPanel::updateMRL() {
    QString mrl = "";
    QString addr = ui.addressText->text();
    int proto = ui.protocolCombo->currentIndex();

    if( addr.contains( "://") && proto != 4 ) {
        mrl = addr;
    } else {
        switch( proto ) {
        case 0:
        case 1:
            mrl = "http://" + addr;
            emit methodChanged("http-caching");
            break;
        case 3:
            mrl = "mms://" + addr;
            emit methodChanged("mms-caching");
            break;
        case 2:
            mrl = "ftp://" + addr;
            emit methodChanged("ftp-caching");
            break;
        case 4: /* RTSP */
            mrl = "rtsp://" + addr;
            emit methodChanged("rtsp-caching");
            break;
        case 5:
            mrl = "udp://@";
            if( ui.ipv6->isEnabled() && ui.ipv6->isChecked() ) {
                mrl += "[::]";
            }
            mrl += QString(":%1").arg( ui.portSpin->value() );
            emit methodChanged("udp-caching");
            break;
        case 6: /* UDP multicast */
            mrl = "udp://@";
            /* Add [] to IPv6 */
            if ( addr.contains(':') && !addr.contains('[') ) {
                mrl += "[" + addr + "]";
            } else mrl += addr;
            mrl += QString(":%1").arg( ui.portSpin->value() );
            emit methodChanged("udp-caching");
        }
    }
    if( ui.timeShift->isEnabled() && ui.timeShift->isChecked() ) {
        mrl += " :access-filter=timeshift";
    }
    emit mrlUpdated( mrl );
}

/**************************************************************************
 * Capture open
 **************************************************************************/
CaptureOpenPanel::CaptureOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    QStackedLayout *stackedDevLayout = new QStackedLayout;
    ui.cardBox->setLayout( stackedDevLayout );

    QStackedLayout *stackedPropLayout = new QStackedLayout;
    ui.optionsBox->setLayout( stackedPropLayout );

#define addModuleAndLayouts( number, name, label )                    \
    QWidget * name ## DevPage = new QWidget( this );                  \
    QWidget * name ## PropPage = new QWidget( this );                 \
    stackedDevLayout->addWidget( name ## DevPage );        \
    stackedPropLayout->addWidget( name ## PropPage );      \
    QGridLayout * name ## DevLayout = new QGridLayout;                \
    QGridLayout * name ## PropLayout = new QGridLayout;               \
    name ## DevPage->setLayout( name ## DevLayout );                  \
    name ## PropPage->setLayout( name ## PropLayout );                \
    ui.deviceCombo->addItem( qtr( label ), QVariant( number ) );

#define CuMRL( widget, slot ) CONNECT( widget , slot , this, updateMRL() );

#define setMaxBound( spinbox ) spinbox->setRange ( 0, INT_MAX );

    /*******
     * V4L *
     *******/
    /* V4l Main */
    addModuleAndLayouts( V4L_DEVICE, v4l, "Video for Linux" );
    QLabel *v4lVideoDeviceLabel = new QLabel( qtr( "Video device name" ) );
    v4lDevLayout->addWidget( v4lVideoDeviceLabel, 0, 0 );
    v4lVideoDevice = new QLineEdit;
    v4lDevLayout->addWidget( v4lVideoDevice, 0, 1 );
    QLabel *v4lAudioDeviceLabel = new QLabel( qtr( "Audio device name" ) );
    v4lDevLayout->addWidget( v4lAudioDeviceLabel, 1, 0 );
    v4lAudioDevice = new QLineEdit;
    v4lDevLayout->addWidget( v4lAudioDevice, 1, 1 );

    /* V4l Props */
    v4lNormBox = new QComboBox;
    v4lNormBox->insertItem( 3, qtr( "Automatic" ) );
    v4lNormBox->insertItem( 0, "SECAM" );
    v4lNormBox->insertItem( 1, "NTSC" );
    v4lNormBox->insertItem( 2, "PAL" );

    v4lFreq = new QSpinBox;
    v4lFreq->setAlignment( Qt::AlignRight );
    v4lFreq->setSuffix(" kHz");

    QLabel *v4lNormLabel = new QLabel( qtr( "Norm" ) );
    QLabel *v4lFreqLabel = new QLabel( qtr( "Frequency" ) );

    v4lPropLayout->addWidget( v4lNormLabel, 0 , 0 );
    v4lPropLayout->addWidget( v4lNormBox, 0 , 1 );

    v4lPropLayout->addWidget( v4lFreqLabel, 1 , 0 );
    v4lPropLayout->addWidget( v4lFreq, 1 , 1 );

    /* v4l CONNECTs */
    CuMRL( v4lVideoDevice, textChanged( QString ) );
    CuMRL( v4lAudioDevice, textChanged( QString ) );
    CuMRL( v4lFreq, valueChanged ( int ) );
    CuMRL( v4lNormBox,  currentIndexChanged ( int ) );

    /************
     * PVR      *
     ************/
    addModuleAndLayouts( PVR_DEVICE, pvr, "PVR" );

    /* PVR Main */
    QLabel *pvrDeviceLabel = new QLabel( qtr( "Device name" ) );
    pvrDevLayout->addWidget( pvrDeviceLabel, 0, 0 );
    pvrDevice = new QLineEdit;
    pvrDevLayout->addWidget( pvrDevice, 0, 1 );
    QLabel *pvrRadioDeviceLabel = new QLabel( qtr( "Radio device name" ) );
    pvrDevLayout->addWidget( pvrRadioDeviceLabel, 1, 0 );
    pvrRadioDevice = new QLineEdit;
    pvrDevLayout->addWidget( pvrRadioDevice, 1, 1 );

    /* PVR props */
    pvrNormBox = new QComboBox;
    pvrNormBox->insertItem( 3, qtr( "Automatic" ) );
    pvrNormBox->insertItem( 0, "SECAM" );
    pvrNormBox->insertItem( 1, "NTSC" );
    pvrNormBox->insertItem( 2, "PAL" );

    pvrFreq = new QSpinBox;
    pvrFreq->setAlignment( Qt::AlignRight );
    pvrFreq->setSuffix(" kHz");
    setMaxBound( pvrFreq );
    pvrBitr = new QSpinBox;
    pvrBitr->setAlignment( Qt::AlignRight );
    pvrBitr->setSuffix(" kHz");
    setMaxBound( pvrBitr );
    QLabel *pvrNormLabel = new QLabel( qtr( "Norm" ) );
    QLabel *pvrFreqLabel = new QLabel( qtr( "Frequency" ) );
    QLabel *pvrBitrLabel = new QLabel( qtr( "Bitrate" ) );

    pvrPropLayout->addWidget( pvrNormLabel, 0, 0 );
    pvrPropLayout->addWidget( pvrNormBox, 0, 1 );

    pvrPropLayout->addWidget( pvrFreqLabel, 1, 0 );
    pvrPropLayout->addWidget( pvrFreq, 1, 1 );

    pvrPropLayout->addWidget( pvrBitrLabel, 2, 0 );
    pvrPropLayout->addWidget( pvrBitr, 2, 1 );

    /* PVR CONNECTs */
    CuMRL( pvrDevice, textChanged( QString ) );
    CuMRL( pvrRadioDevice, textChanged( QString ) );

    CuMRL( pvrFreq, valueChanged ( int ) );
    CuMRL( pvrBitr, valueChanged ( int ) );
    CuMRL( pvrNormBox,  currentIndexChanged ( int ) );

    /*********************
     * DirectShow Stuffs *
     *********************/
    addModuleAndLayouts( DSHOW_DEVICE, dshow, "DirectShow" );


    /**************
     * BDA Stuffs *
     **************/
    addModuleAndLayouts( BDA_DEVICE, bda, "DVB DirectShow" );

    /* bda Main */
    QLabel *bdaDeviceLabel = new QLabel( qtr( "Adapter card to tune" ) );
    QLabel *bdaTypeLabel = new QLabel( qtr( "DVB Type:" ) );

    bdaCard = new QSpinBox;
    bdaCard->setAlignment( Qt::AlignRight );

    bdaDevLayout->addWidget( bdaDeviceLabel, 0, 0 );
    bdaDevLayout->addWidget( bdaCard, 0, 2, 1, 2 );

    bdas = new QRadioButton( "DVB-S" );
    bdas->setChecked( true );
    bdac = new QRadioButton( "DVB-C" );
    bdat = new QRadioButton( "DVB-T" );

    bdaDevLayout->addWidget( bdaTypeLabel, 1, 0 );
    bdaDevLayout->addWidget( bdas, 1, 1 );
    bdaDevLayout->addWidget( bdac, 1, 2 );
    bdaDevLayout->addWidget( bdat, 1, 3 );

    /* bda Props */
    QLabel *bdaFreqLabel =
                    new QLabel( qtr( "Transponder/multiplex frequency" ) );
    bdaFreq = new QSpinBox;
    bdaFreq->setAlignment( Qt::AlignRight );
    bdaFreq->setSuffix(" kHz");
    setMaxBound( bdaFreq )
    bdaPropLayout->addWidget( bdaFreqLabel, 0, 0 );
    bdaPropLayout->addWidget( bdaFreq, 0, 1 );

    bdaSrateLabel = new QLabel( qtr( "Transponder symbol rate" ) );
    bdaSrate = new QSpinBox;
    bdaSrate->setAlignment( Qt::AlignRight );
    bdaSrate->setSuffix(" kHz");
    setMaxBound( bdaSrate );
    bdaPropLayout->addWidget( bdaSrateLabel, 1, 0 );
    bdaPropLayout->addWidget( bdaSrate, 1, 1 );

    /* bda CONNECTs */
    CuMRL( bdaCard, valueChanged ( int ) );
    CuMRL( bdaFreq, valueChanged ( int ) );
    CuMRL( bdaSrate, valueChanged ( int ) );
    BUTTONACT( bdas, updateButtons() );
    BUTTONACT( bdat, updateButtons() );
    BUTTONACT( bdac, updateButtons() );
    BUTTONACT( bdas, updateMRL() );
    BUTTONACT( bdat, updateMRL() );
    BUTTONACT( bdac, updateMRL() );

    /**************
     * DVB Stuffs *
     **************/
    addModuleAndLayouts( DVB_DEVICE, dvb, "DVB" );

    /* DVB Main */
    QLabel *dvbDeviceLabel = new QLabel( qtr( "Adapter card to tune" ) );
    QLabel *dvbTypeLabel = new QLabel( qtr( "DVB Type:" ) );

    dvbCard = new QSpinBox;
    dvbCard->setAlignment( Qt::AlignRight );
    dvbCard->setPrefix( "/dev/dvb/adapter" );

    dvbDevLayout->addWidget( dvbDeviceLabel, 0, 0 );
    dvbDevLayout->addWidget( dvbCard, 0, 2, 1, 2 );

    dvbs = new QRadioButton( "DVB-S" );
    dvbs->setChecked( true );
    dvbc = new QRadioButton( "DVB-C" );
    dvbt = new QRadioButton( "DVB-T" );

    dvbDevLayout->addWidget( dvbTypeLabel, 1, 0 );
    dvbDevLayout->addWidget( dvbs, 1, 1 );
    dvbDevLayout->addWidget( dvbc, 1, 2 );
    dvbDevLayout->addWidget( dvbt, 1, 3 );

    /* DVB Props */
    QLabel *dvbFreqLabel =
                    new QLabel( qtr( "Transponder/multiplex frequency" ) );
    dvbFreq = new QSpinBox;
    dvbFreq->setAlignment( Qt::AlignRight );
    dvbFreq->setSuffix(" kHz");
    setMaxBound( dvbFreq  );
    dvbPropLayout->addWidget( dvbFreqLabel, 0, 0 );
    dvbPropLayout->addWidget( dvbFreq, 0, 1 );

    QLabel *dvbSrateLabel = new QLabel( qtr( "Transponder symbol rate" ) );
    dvbSrate = new QSpinBox;
    dvbSrate->setAlignment( Qt::AlignRight );
    dvbSrate->setSuffix(" kHz");
    setMaxBound( dvbSrate );
    dvbPropLayout->addWidget( dvbSrateLabel, 1, 0 );
    dvbPropLayout->addWidget( dvbSrate, 1, 1 );

    /* DVB CONNECTs */
    CuMRL( dvbCard, valueChanged ( int ) );
    CuMRL( dvbFreq, valueChanged ( int ) );
    CuMRL( dvbSrate, valueChanged ( int ) );
    BUTTONACT( dvbs, updateButtons() );
    BUTTONACT( dvbt, updateButtons() );
    BUTTONACT( dvbc, updateButtons() );

    /* General connects */
    connect( ui.deviceCombo, SIGNAL( activated( int ) ),
                     stackedDevLayout, SLOT( setCurrentIndex( int ) ) );
    connect( ui.deviceCombo, SIGNAL( activated( int ) ),
                     stackedPropLayout, SLOT( setCurrentIndex( int ) ) );
    CONNECT( ui.deviceCombo, activated( int ), this, updateMRL() );

#undef addModule
}

CaptureOpenPanel::~CaptureOpenPanel()
{}

void CaptureOpenPanel::clear()
{}

void CaptureOpenPanel::updateMRL()
{
    QString mrl = "";
    int i_devicetype = ui.deviceCombo->itemData(
            ui.deviceCombo->currentIndex() ).toInt();
    msg_Dbg( p_intf, "Capture Type: %i", i_devicetype );
    switch( i_devicetype )
    {
    case V4L_DEVICE:
        mrl = "v4l://";
        mrl += " :v4l-vdev=" + v4lVideoDevice->text();
        mrl += " :v4l-adev=" + v4lAudioDevice->text();
        mrl += " :v4l-norm=" + QString("%1").arg( v4lNormBox->currentIndex() );
        mrl += " :v4l-frequency=" + QString("%1").arg( v4lFreq->value() );
        break;
    case PVR_DEVICE:
        mrl = "pvr://";
        mrl += " :pvr-device=" + pvrDevice->text();
        mrl += " :pvr-radio-device=" + pvrRadioDevice->text();
        mrl += " :pvr-norm=" + QString("%1").arg( pvrNormBox->currentIndex() );
        if( pvrFreq->value() )
            mrl += " :pvr-frequency=" + QString("%1").arg( pvrFreq->value() );
        if( pvrBitr->value() )
            mrl += " :pvr-bitrate=" + QString("%1").arg( pvrBitr->value() );
        break;
    case DVB_DEVICE:
        mrl = "dvb://";
        mrl += " :dvb-adapter=" + QString("%1").arg( dvbCard->value() );
        mrl += " :dvb-frequency=" + QString("%1").arg( dvbFreq->value() );
        mrl += " :dvb-srate=" + QString("%1").arg( dvbSrate->value() );
        break;
    case BDA_DEVICE:
        if( bdas->isChecked() ) mrl = "dvb-s://";
        else if(  bdat->isChecked() ) mrl = "dvb-t://";
        else if(  bdac->isChecked() ) mrl = "dvb-c://";
        else return;
        mrl += " :dvb-adapter=" + QString("%1").arg( bdaCard->value() );
        mrl += " :dvb-frequency=" + QString("%1").arg( bdaFreq->value() );
        mrl += " :dvb-srate=" + QString("%1").arg( bdaSrate->value() );
        break;
  case DSHOW_DEVICE:
        break;
    }

    emit mrlUpdated( mrl );
}

void CaptureOpenPanel::updateButtons()
{
    int i_devicetype = ui.deviceCombo->itemData(
            ui.deviceCombo->currentIndex() ).toInt();
    msg_Dbg( p_intf, "Capture Type: %i", i_devicetype );
    switch( i_devicetype )
    {
    case DVB_DEVICE:
        if( dvbs->isChecked() ) dvbFreq->setSuffix(" kHz");
        if( dvbc->isChecked() || dvbt->isChecked() ) dvbFreq->setSuffix(" Hz");
        break;
    case BDA_DEVICE:
        if( bdas->isChecked() ) bdaFreq->setSuffix(" kHz");
        if( bdac->isChecked() || bdat->isChecked() ) bdaFreq->setSuffix(" Hz");
        if( bdas->isChecked() || bdac->isChecked() )
        {
            bdaSrate->show();
            bdaSrateLabel->show();
        }
        else
        {
            bdaSrate->show();
            bdaSrateLabel->hide();
        }
        break;
    }
}
