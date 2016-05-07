/** *************************************************************************
                          kstarslite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 30/04/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarslite.h"
#include "skymaplite.h"
#include "kstarsdata.h"
#include <QQmlContext>

#include "Options.h"
#include "ksutils.h"

KStarsLite *KStarsLite::pinstance = 0;

KStarsLite::KStarsLite( bool doSplash, bool startClock, const QString &startDateString) {
    // Initialize logging settings
    if (Options::disableLogging())
        KSUtils::Logging::Disable();
    else if (Options::logToFile() && Options::verboseLogFile().isEmpty() == false)
        KSUtils::Logging::UseFile(Options::verboseLogFile());
    else
        KSUtils::Logging::UseDefault();

    //Register SkyMapLite for use within QML
    qmlRegisterType<SkyMapLite>("skymaplite",1,0,"SkyMapLite");
    QString main = QString(SOURCE_DIR) + "/kstars/kstarslite/qml/main.qml";

    m_Engine.load(QUrl(main));

    m_RootObject = m_Engine.rootObjects()[0];
    Q_ASSERT(m_RootObject);
    if(!m_RootObject) qDebug() << "Please, check QML files for syntax errors";

    QQuickItem* skyMapLiteWrapper = m_RootObject->findChild<QQuickItem*>("skyMapLiteWrapper");
    //Once KStarsData is loaded we are ready to show the SkyMapLite

    m_KStarsData = KStarsData::Create();
    Q_ASSERT( m_KStarsData );

    //Make instance of KStarsLite and KStarsData available to QML
    m_Engine.rootContext()->setContextProperty("KStarsLite", this);
    m_Engine.rootContext()->setContextProperty("KStarsData", m_KStarsData);

    //Set Geographic Location from Options
    m_KStarsData->setLocationFromOptions();

    /*SkyMapLite has to be loaded before KStarsData is initialized because SkyComponents derived classes
    have to add SkyItems to the SkyMapLite*/
    m_SkyMapLite = SkyMapLite::createInstance(skyMapLiteWrapper);

    // Set pinstance to yourself
    pinstance = this;

    //Initialize Time and Date
    if (startDateString.isEmpty() == false)
    {
        KStarsDateTime startDate = KStarsDateTime::fromString( startDateString );
        if (startDate.isValid() )
            data()->changeDateTime( data()->geo()->LTtoUT( startDate ) );
        else
            data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
    }
    else data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );

    // Initialize clock. If --paused is not in the comand line, look in options
    if ( startClock ) StartClockRunning =  Options::runClock();

    // Setup splash screen
    if ( doSplash ) {
        //connect( m_KStarsData, SIGNAL( progressText(QString) ), splash, SLOT( setMessage(QString) ));
        //splash->show();
    } else {
        connect( m_KStarsData, SIGNAL( progressText(QString) ), m_KStarsData, SLOT( slotConsoleMessage(QString) ) );
    }

    //set up Dark color scheme for application windows
    //TODO: Move that to QML
    DarkPalette = QPalette(QColor("darkred"), QColor("darkred"));
    DarkPalette.setColor( QPalette::Normal, QPalette::Base, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Highlight, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::HighlightedText, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Base, QColor( 30, 10, 10 ) );
    //store original color scheme
    OriginalPalette = QApplication::palette();
    if( !m_KStarsData->initialize() ) return;
    datainitFinished();

#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1  && !defined(__UCLIBC__) )
    qDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()";
#else
    qDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions.";
#endif
}

KStarsLite *KStarsLite::createInstance( bool doSplash, bool clockrunning, const QString &startDateString) {
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStarsLite( doSplash, clockrunning, startDateString );
    Q_ASSERT( pinstance && "pinstance must be non NULL");
    return nullptr;
}

void KStarsLite::updateTime( const bool automaticDSTchange ) {
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime( Data->geo(), automaticDSTchange );

    //We do this outside of kstarsdata just to get the coordinates
    //displayed in the infobox to update every second.
    //	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
    //		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
    //		Map->focus()->setRA( Map->focus()->ra().Hours() + double( nSec )/3600. );
    //		if ( Options::useAltAz() ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
    //		Map->showFocusCoords();
    //	}

    //If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
    //so that it can be ticked manually after each update, in order to make each time
    //step exactly equal to the timeScale setting.
    //Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
    //the skymap has been completely updated.
    if ( Data->clock()->isManualMode() && Data->clock()->isActive() ) {
        QTimer::singleShot( 0, Data->clock(), SLOT( manualTick() ) );
    }
}
