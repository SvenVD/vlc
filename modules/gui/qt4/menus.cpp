/*****************************************************************************
 * menus.cpp : Qt menus
 *****************************************************************************
 * Copyright © 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

/** \todo
 * - Remove static currentGroup
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_intf_strings.h>
#include <vlc_services_discovery.h>

#include "menus.hpp"

#include "main_interface.hpp"    /* View modifications */
#include "dialogs_provider.hpp"  /* Dialogs display */
#include "input_manager.hpp"     /* Input Management */
#include "recents.hpp"           /* Recent Items */
#include "actions_manager.hpp"

#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QSignalMapper>
#include <QSystemTrayIcon>
#include <QList>

/*
  This file defines the main menus and the pop-up menu (right-click menu)
  and the systray menu (in that order in the file)

  There are 3 menus that have to be rebuilt everytime there are called:
  Audio, Video, Navigation
  3 functions are building those menus: AudioMenu, VideoMenu, NavigMenu
  and 3 functions associated are collecting the objects :
  InputAutoMenuBuilder, AudioAutoMenuBuilder, VideoAutoMenuBuilder.

  A QSignalMapper decides when to rebuild those menus cf MenuFunc in the .hpp
  Just before one of those menus are aboutToShow(), they are rebuild.
  */

enum
{
    ITEM_NORMAL,
    ITEM_CHECK,
    ITEM_RADIO
};

static QActionGroup *currentGroup;

/* HACK for minimalView to go around a Qt bug/feature
 * that doesn't update the QAction checked state when QMenu is hidden */
QAction *QVLCMenu::minimalViewAction = NULL;
QAction *QVLCMenu::fullscreenViewAction = NULL;

QMenu *QVLCMenu::recentsMenu = NULL;

/****************************************************************************
 * Menu code helpers:
 ****************************************************************************
 * Add static entries to DP in menus
 ***************************************************************************/
void addDPStaticEntry( QMenu *menu,
                       const QString text,
                       const char *icon,
                       const char *member,
                       const char *shortcut = NULL )
{
    QAction *action = NULL;
    if( !EMPTY_STR( icon ) > 0 )
    {
        if( !EMPTY_STR( shortcut ) > 0 )
            action = menu->addAction( QIcon( icon ), text, THEDP,
                                      member, qtr( shortcut ) );
        else
            action = menu->addAction( QIcon( icon ), text, THEDP, member );
    }
    else
    {
        if( !EMPTY_STR( shortcut ) > 0 )
            action = menu->addAction( text, THEDP, member, qtr( shortcut ) );
        else
            action = menu->addAction( text, THEDP, member );
    }
    action->setData( true );
}

/**
 * @brief Enable all static entries, disable the others
 * @param enable if false, disable all entries
 */
void EnableStaticEntries( QMenu *menu, bool enable = true )
{
    if( !menu ) return;

    QList< QAction* > actions = menu->actions();
    for( int i = 0; i < actions.size(); ++i )
    {
        actions[i]->setEnabled( enable && actions[i]->data().toBool() );
    }
}

/**
 * \return Number of static entries
 */
int DeleteNonStaticEntries( QMenu *menu )
{
    int i_ret = 0;
    if( !menu )
        return VLC_EGENERIC;
    QList< QAction* > actions = menu->actions();
    for( int i = 0; i < actions.size(); ++i )
    {
        if( !actions[i]->data().toBool() )
            delete actions[i];
        else
            i_ret++;
    }
    return i_ret;
}

/***
 * Same for MIM
 ***/
void addMIMStaticEntry( intf_thread_t *p_intf,
                        QMenu *menu,
                        const QString text,
                        const char *icon,
                        const char *member )
{
    if( strlen( icon ) > 0 )
    {
        QAction *action = menu->addAction( text, THEMIM,  member );
        action->setIcon( QIcon( icon ) );
    }
    else
    {
        menu->addAction( text, THEMIM, member );
    }
}

/*****************************************************************************
 * Definitions of variables for the dynamic menus
 *****************************************************************************/
#define PUSH_VAR( var ) varnames.push_back( var ); \
    objects.push_back( VLC_OBJECT(p_object) )

#define PUSH_INPUTVAR( var ) varnames.push_back( var ); \
    objects.push_back( VLC_OBJECT(p_input) );

#define PUSH_SEPARATOR if( objects.size() != i_last_separator ) { \
    objects.push_back( 0 ); varnames.push_back( "" ); \
    i_last_separator = objects.size(); }

static int InputAutoMenuBuilder( input_thread_t *p_object,
        vector<vlc_object_t *> &objects,
        vector<const char *> &varnames )
{
    PUSH_VAR( "bookmark" );
    PUSH_VAR( "title" );
    PUSH_VAR( "chapter" );
    PUSH_VAR( "navigation" );
    PUSH_VAR( "program" );
    PUSH_VAR( "dvd_menus" );
    return VLC_SUCCESS;
}

static int VideoAutoMenuBuilder( vout_thread_t *p_object,
        input_thread_t *p_input,
        vector<vlc_object_t *> &objects,
        vector<const char *> &varnames )
{
    PUSH_INPUTVAR( "video-es" );
    PUSH_INPUTVAR( "spu-es" );
    PUSH_VAR( "fullscreen" );
    PUSH_VAR( "video-on-top" );
#ifdef WIN32
    PUSH_VAR( "directx-wallpaper" );
#endif
    PUSH_VAR( "video-snapshot" );
    PUSH_VAR( "zoom" );
    PUSH_VAR( "autoscale" );
    PUSH_VAR( "aspect-ratio" );
    PUSH_VAR( "crop" );
    PUSH_VAR( "deinterlace" );
    PUSH_VAR( "postprocess" );

    return VLC_SUCCESS;
}

static int AudioAutoMenuBuilder( aout_instance_t *p_object,
        input_thread_t *p_input,
        vector<vlc_object_t *> &objects,
        vector<const char *> &varnames )
{
    PUSH_INPUTVAR( "audio-es" );
    PUSH_VAR( "audio-channels" );
    PUSH_VAR( "audio-device" );
    PUSH_VAR( "visual" );
    return VLC_SUCCESS;
}

static QAction * FindActionWithVar( QMenu *menu, const char *psz_var )
{
    QList< QAction* > actions = menu->actions();
    for( int i = 0; i < actions.size(); ++i )
    {
        if( actions[i]->data().toString() == psz_var )
            return actions[i];
    }
    return NULL;
}

/*****************************************************************************
 * All normal menus
 * Simple Code
 *****************************************************************************/

#define BAR_ADD( func, title ) { \
    QMenu *_menu = func; _menu->setTitle( title ); bar->addMenu( _menu ); }

#define BAR_DADD( func, title, id ) { \
    QMenu *_menu = func; _menu->setTitle( title ); bar->addMenu( _menu ); \
    MenuFunc *f = new MenuFunc( _menu, id ); \
    CONNECT( _menu, aboutToShow(), THEDP->menusUpdateMapper, map() ); \
    THEDP->menusUpdateMapper->setMapping( _menu, f ); }

#define ACT_ADD( _menu, val, title ) { \
    QAction *_action = new QAction( title, _menu ); _action->setData( val ); \
    _menu->addAction( _action ); }

#define ACT_ADDMENU( _menu, val, title ) { \
    QAction *_action = new QAction( title, _menu ); _action->setData( val ); \
    _action->setMenu( new QMenu() ); _menu->addAction( _action ); }

#define ACT_ADDCHECK( _menu, val, title ) { \
    QAction *_action = new QAction( title, _menu ); _action->setData( val ); \
    _action->setCheckable( true ); _menu->addAction( _action ); }

/**
 * Main Menu Bar Creation
 **/
void QVLCMenu::createMenuBar( MainInterface *mi,
                              intf_thread_t *p_intf,
                              bool visual_selector_enabled )
/* FIXME remove this visual dependency */
{
    /* QMainWindows->menuBar()
       gives the QProcess::destroyed timeout issue on Cleanlooks style with
       setDesktopAware set to false */
    QMenuBar *bar = mi->menuBar();
    BAR_ADD( FileMenu( p_intf, bar ), qtr( "&Media" ) );

    BAR_DADD( NavigMenu( p_intf, bar ), qtr( "P&layback" ), 3 );
    BAR_DADD( AudioMenu( p_intf, bar ), qtr( "&Audio" ), 1 );
    BAR_DADD( VideoMenu( p_intf, bar ), qtr( "&Video" ), 2 );

    BAR_ADD( ToolsMenu( bar ), qtr( "&Tools" ) );
    BAR_ADD( ViewMenu( p_intf, NULL, mi, visual_selector_enabled, true ),
             qtr( "V&iew" ) );

    BAR_ADD( HelpMenu( bar ), qtr( "&Help" ) );
}
#undef BAR_ADD
#undef BAR_DADD

/**
 * Media ( File ) Menu
 * Opening, streaming and quit
 **/
QMenu *QVLCMenu::FileMenu( intf_thread_t *p_intf, QWidget *parent )
{
    QMenu *menu = new QMenu( parent );

    addDPStaticEntry( menu, qtr( "&Open File..." ),
        ":/file-asym", SLOT( simpleOpenDialog() ), "Ctrl+O" );
    addDPStaticEntry( menu, qtr( "Advanced Open File..." ),
        ":/file-asym", SLOT( openFileDialog() ), "Ctrl+Shift+O" );
    addDPStaticEntry( menu, qtr( I_OPEN_FOLDER ),
        ":/folder-grey", SLOT( PLOpenDir() ), "Ctrl+F" );
    addDPStaticEntry( menu, qtr( "Open &Disc..." ),
        ":/disc", SLOT( openDiscDialog() ), "Ctrl+D" );
    addDPStaticEntry( menu, qtr( "Open &Network..." ),
        ":/network", SLOT( openNetDialog() ), "Ctrl+N" );
    addDPStaticEntry( menu, qtr( "Open &Capture Device..." ),
        ":/capture-card", SLOT( openCaptureDialog() ),
        "Ctrl+C" );

    menu->addSeparator();
    addDPStaticEntry( menu, qtr( "Paste &MRL" ),
                      NULL, SLOT( openUrlDialog() ), "Ctrl+V" );

    recentsMenu = new QMenu( qtr( "&Recent Media" ), menu );
    updateRecents( p_intf );
    menu->addMenu( recentsMenu );
    menu->addSeparator();
    menu->addMenu( SDMenu( p_intf, menu ) );
    menu->addSeparator();

    addDPStaticEntry( menu, qtr( I_PL_SAVE ), "", SLOT( saveAPlaylist() ),
        "Ctrl+Y" );
    menu->addSeparator();

    addDPStaticEntry( menu, qtr( "Conve&rt / Save..." ), "",
        SLOT( openAndTranscodingDialogs() ), "Ctrl+R" );
    addDPStaticEntry( menu, qtr( "&Streaming..." ),
        ":/stream", SLOT( openAndStreamingDialogs() ),
        "Ctrl+S" );
    menu->addSeparator();

    addDPStaticEntry( menu, qtr( "&Quit" ) ,
        ":/quit", SLOT( quit() ), "Ctrl+Q" );
    return menu;
}

/* Playlist/MediaLibrary Control */
QMenu *QVLCMenu::ToolsMenu( QMenu *menu )
{
    addDPStaticEntry( menu, qtr( I_MENU_EXT ), ":/settings",
            SLOT( extendedDialog() ), "Ctrl+E" );

    addDPStaticEntry( menu, qtr( I_MENU_INFO ) , ":/info",
        SLOT( mediaInfoDialog() ), "Ctrl+I" );
    addDPStaticEntry( menu, qtr( I_MENU_CODECINFO ) ,
        ":/info", SLOT( mediaCodecDialog() ), "Ctrl+J" );

    addDPStaticEntry( menu, qtr( I_MENU_BOOKMARK ),"",
                      SLOT( bookmarksDialog() ), "Ctrl+B" );
#ifdef ENABLE_VLM
    addDPStaticEntry( menu, qtr( I_MENU_VLM ), "", SLOT( vlmDialog() ),
        "Ctrl+W" );
#endif

    addDPStaticEntry( menu, qtr( I_MENU_MSG ),
        ":/messages", SLOT( messagesDialog() ),
        "Ctrl+M" );

    addDPStaticEntry( menu, qtr( "Plu&gins and extensions" ),
        "", SLOT( pluginDialog() ) );
    menu->addSeparator();

    addDPStaticEntry( menu, qtr( "&Preferences" ),
        ":/preferences", SLOT( prefsDialog() ), "Ctrl+P" );

    return menu;
}

QMenu *QVLCMenu::ToolsMenu( QWidget *parent )
{
    return ToolsMenu( new QMenu( parent ) );
}

/**
 * View Menu
 * This menu can be an interface menu but also a right click menu.
 **/
QMenu *QVLCMenu::ViewMenu( intf_thread_t *p_intf,
                            QMenu *current,
                            MainInterface *mi,
                            bool visual_selector_enabled,
                            bool with_intf )
{
    QMenu *menu;
    if( current )
        menu = new QMenu( current );
    else
        menu = new QMenu( mi );
    QAction *act;
    if( mi )
    {
        act = menu->addAction( QIcon( ":/playlist_menu" ),
                               qtr( "Play&list" ), mi,
                               SLOT( togglePlaylist() ), qtr( "Ctrl+L" ) );
        act->setData( true );
    }
    /*menu->addSeparator();
    menu->addAction( qtr( "Undock from Interface" ), mi,
                     SLOT( undockPlaylist() ), qtr( "Ctrl+U" ) );*/


    menu->addSeparator();

    if( with_intf )
    {
        QMenu *intfmenu = InterfacesMenu( p_intf, menu );
        MenuFunc *f = new MenuFunc( intfmenu, 4 );
        CONNECT( intfmenu, aboutToShow(), THEDP->menusUpdateMapper, map() );
        THEDP->menusUpdateMapper->setMapping( intfmenu, f );
        menu->addSeparator();
    }
    if( mi )
    {
        /* Minimal View */
        QAction *action = menu->addAction( qtr( "Mi&nimal View" ), mi,
                                SLOT( toggleMinimalView() ), qtr( "Ctrl+H" ) );
        action->setCheckable( true );
        action->setData( true );
        if( mi->getControlsVisibilityStatus() & CONTROLS_VISIBLE )
            action->setChecked( true );
        minimalViewAction = action; /* HACK for minimalView */

        /* FullScreen View */
        action = menu->addAction( qtr( "&Fullscreen Interface" ), mi,
                                  SLOT( toggleFullScreen() ), QString( "F11" ) );
        fullscreenViewAction = action;
        action->setCheckable( true );
        action->setData( true );

        /* Advanced Controls */
        action = menu->addAction( qtr( "&Advanced Controls" ), mi,
                                  SLOT( toggleAdvanced() ) );
        action->setCheckable( true );
        action->setData( true );
        if( mi->getControlsVisibilityStatus() & CONTROLS_ADVANCED )
            action->setChecked( true );
#if 0 /* For Visualisations. Not yet working */
        adv = menu->addAction( qtr( "Visualizations selector" ),
                mi, SLOT( visual() ) );
        adv->setCheckable( true );
        if( visual_selector_enabled ) adv->setChecked( true );
#endif
    }

    menu->addSeparator();
    addDPStaticEntry( menu, qtr( "Customi&ze Interface..." ),
        ":/preferences", SLOT( toolbarDialog() ) );
    menu->addSeparator();

    return menu;
}

/**
 * Interface Sub-Menu, to list extras interface and skins
 **/
QMenu *QVLCMenu::InterfacesMenu( intf_thread_t *p_intf, QMenu *current )
{
    vector<vlc_object_t *> objects;
    vector<const char *> varnames;
    /** \todo add "switch to XXX" */
    varnames.push_back( "intf-add" );
    objects.push_back( VLC_OBJECT(p_intf) );

    return Populate( p_intf, current, varnames, objects );
}

/**
 * Main Audio Menu
 */
QMenu *QVLCMenu::AudioMenu( intf_thread_t *p_intf, QMenu * current )
{
    vector<vlc_object_t *> objects;
    vector<const char *> varnames;
    aout_instance_t *p_aout;
    input_thread_t *p_input;

    if( current->isEmpty() )
    {
        ACT_ADDMENU( current, "audio-es", qtr( "Audio &Track" ) );
        ACT_ADDMENU( current, "audio-channels", qtr( "Audio &Channels" ) );
        ACT_ADDMENU( current, "audio-device", qtr( "Audio &Device" ) );
        current->addSeparator();

        ACT_ADDMENU( current, "visual", qtr( "&Visualizations" ) );
        current->addSeparator();

        QAction *action = current->addAction( qtr( "Increase Volume" ),
                ActionsManager::getInstance( p_intf ), SLOT( AudioUp() ) );
        action->setData( true );
        action = current->addAction( qtr( "Decrease Volume" ),
                ActionsManager::getInstance( p_intf ), SLOT( AudioDown() ) );
        action->setData( true );
        action = current->addAction( qtr( "Mute" ),
                ActionsManager::getInstance( p_intf ), SLOT( toggleMuteAudio() ) );
        action->setData( true );
    }

    p_input = THEMIM->getInput();
    if( p_input )
        vlc_object_hold( p_input );
    p_aout = THEMIM->getAout();
    AudioAutoMenuBuilder( p_aout, p_input, objects, varnames );
    EnableStaticEntries( current, ( p_aout != NULL ) );
    if( p_aout )
    {
        vlc_object_release( p_aout );
    }
    if( p_input )
        vlc_object_release( p_input );

    return Populate( p_intf, current, varnames, objects );
}

QMenu *QVLCMenu::AudioMenu( intf_thread_t *p_intf, QWidget *parent )
{
    return AudioMenu( p_intf, new QMenu( parent ) );
}

/**
 * Main Video Menu
 * Subtitles are part of Video.
 **/
QMenu *QVLCMenu::VideoMenu( intf_thread_t *p_intf, QMenu *current )
{
    vout_thread_t *p_vout;
    input_thread_t *p_input;
    vector<vlc_object_t *> objects;
    vector<const char *> varnames;

    if( current->isEmpty() )
    {
        ACT_ADDMENU( current, "video-es", qtr( "Video &Track" ) );

        QAction *action;
        QMenu *submenu = new QMenu( qtr( "&Subtitles Track" ), current );
        action = current->addMenu( submenu );
        action->setData( "spu-es" );
        addDPStaticEntry( submenu, qtr( "Open File..." ), "",
                          SLOT( loadSubtitlesFile() ) );
        submenu->addSeparator();
        current->addSeparator();

        ACT_ADDCHECK( current, "fullscreen", qtr( "&Fullscreen" ) );
        ACT_ADDCHECK( current, "video-on-top", qtr( "Always &On Top" ) );
#ifdef WIN32
        ACT_ADDCHECK( current, "directx-wallpaper", qtr( "DirectX Wallpaper" ) );
#endif
        ACT_ADD( current, "video-snapshot", qtr( "Sna&pshot" ) );

        current->addSeparator();

        ACT_ADDMENU( current, "zoom", qtr( "&Zoom" ) );
        ACT_ADDCHECK( current, "autoscale", qtr( "Sca&le" ) );
        ACT_ADDMENU( current, "aspect-ratio", qtr( "&Aspect Ratio" ) );
        ACT_ADDMENU( current, "crop", qtr( "&Crop" ) );
        ACT_ADDMENU( current, "deinterlace", qtr( "&Deinterlace" ) );
        ACT_ADDMENU( current, "postprocess", qtr( "&Post processing" ) );
    }

    p_input = THEMIM->getInput();
    if( p_input )
        vlc_object_hold( p_input );

    p_vout = THEMIM->getVout();
    VideoAutoMenuBuilder( p_vout, p_input, objects, varnames );
    EnableStaticEntries( current, ( p_vout != NULL ) );
    if( p_vout )
    {
        vlc_object_release( p_vout );
    }
    if( p_input )
        vlc_object_release( p_input );

    return Populate( p_intf, current, varnames, objects );
}

QMenu *QVLCMenu::VideoMenu( intf_thread_t *p_intf, QWidget *parent )
{
    return VideoMenu( p_intf, new QMenu( parent ) );
}

/**
 * Navigation Menu
 * For DVD, MP4, MOV and other chapter based format
 **/
QMenu *QVLCMenu::NavigMenu( intf_thread_t *p_intf, QMenu *menu )
{
    if( menu->isEmpty() )
    {
        QAction *action;
        QMenu *submenu = new QMenu( qtr( "&Bookmarks" ), menu );
        addDPStaticEntry( submenu, qtr( "Manage &bookmarks" ), "",
                          SLOT( bookmarksDialog() ) );
        submenu->addSeparator();
        action = menu->addMenu( submenu );
        action->setData( "bookmark" );
        ACT_ADDMENU( menu, "title", qtr( "T&itle" ) );
        ACT_ADDMENU( menu, "chapter", qtr( "&Chapter" ) );
        ACT_ADDMENU( menu, "navigation", qtr( "&Navigation" ) );
        ACT_ADDMENU( menu, "program", qtr( "&Program" ) );

        menu->addSeparator();
        addDPStaticEntry( menu, qtr( I_MENU_GOTOTIME ),"",
                          SLOT( gotoTimeDialog() ), "Ctrl+T" );

    }

    input_thread_t *p_object;
    vector<vlc_object_t *> objects;
    vector<const char *> varnames;

    p_object = THEMIM->getInput();
    if( p_object )
        vlc_object_hold( p_object );
    InputAutoMenuBuilder( p_object, objects, varnames );
    PUSH_VAR( "prev-title" );
    PUSH_VAR( "next-title" );
    PUSH_VAR( "prev-chapter" );
    PUSH_VAR( "next-chapter" );
    EnableStaticEntries( menu, ( p_object != NULL ) );
    if( p_object )
    {
        vlc_object_release( p_object );
    }
    return Populate( p_intf, menu, varnames, objects );
}

QMenu *QVLCMenu::NavigMenu( intf_thread_t *p_intf, QWidget *parent )
{
    return NavigMenu( p_intf, new QMenu( parent ) );
}

/**
 * Service Discovery SubMenu
 **/
QMenu *QVLCMenu::SDMenu( intf_thread_t *p_intf, QWidget *parent )
{
    QMenu *menu = new QMenu( parent );
    menu->setTitle( qtr( I_PL_SD ) );
    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( &ppsz_longnames );
    if( !ppsz_names )
        return menu;

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        QAction *a = new QAction( qfu( *ppsz_longname ), menu );
        a->setCheckable( true );
        if( playlist_IsServicesDiscoveryLoaded( THEPL, *ppsz_name ) )
            a->setChecked( true );
        CONNECT( a , triggered(), THEDP->SDMapper, map() );
        THEDP->SDMapper->setMapping( a, QString( *ppsz_name ) );
        menu->addAction( a );

        if( !strcmp( *ppsz_name, "podcast" ) )
        {
            QAction *b = new QAction( qtr( "Configure podcasts..." ), menu );
            //b->setEnabled( a->isChecked() );
            menu->addAction( b );
            CONNECT( b, triggered(), THEDP, podcastConfigureDialog() );
        }
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );
    return menu;
}

/**
 * Help/About Menu
**/
QMenu *QVLCMenu::HelpMenu( QWidget *parent )
{
    QMenu *menu = new QMenu( parent );
    addDPStaticEntry( menu, qtr( "&Help..." ) ,
        ":/help", SLOT( helpDialog() ), "F1" );
#ifdef UPDATE_CHECK
    addDPStaticEntry( menu, qtr( "Check for &Updates..." ) , "",
                      SLOT( updateDialog() ) );
#endif
    menu->addSeparator();
    addDPStaticEntry( menu, qtr( I_MENU_ABOUT ), ":/info",
            SLOT( aboutDialog() ), "Shift+F1" );
    return menu;
}

/*****************************************************************************
 * Popup menus - Right Click menus                                           *
 *****************************************************************************/
#define POPUP_BOILERPLATE \
    unsigned int i_last_separator = 0; \
    vector<vlc_object_t *> objects; \
    vector<const char *> varnames; \
    input_thread_t *p_input = THEMIM->getInput();

#define CREATE_POPUP \
    Populate( p_intf, menu, varnames, objects ); \
    p_intf->p_sys->p_popup_menu = menu; \
    menu->popup( QCursor::pos() ); \
    p_intf->p_sys->p_popup_menu = NULL; \
    i_last_separator = 0;

void QVLCMenu::PopupMenuControlEntries( QMenu *menu,
                                        intf_thread_t *p_intf,
                                        input_thread_t *p_input )
{
    if( p_input )
    {
        vlc_value_t val;
        var_Get( p_input, "state", &val );
        if( val.i_int == PLAYING_S )
            addMIMStaticEntry( p_intf, menu, qtr( "Pause" ),
                    ":/pause", SLOT( togglePlayPause() ) );
        else
            addMIMStaticEntry( p_intf, menu, qtr( "Play" ),
                    ":/play", SLOT( togglePlayPause() ) );
    }
    else if( THEPL->items.i_size )
        addMIMStaticEntry( p_intf, menu, qtr( "Play" ),
                ":/play", SLOT( togglePlayPause() ) );
    else
        addDPStaticEntry( menu, qtr( "Play" ),
                ":/play", SLOT( openDialog() ) );

    addMIMStaticEntry( p_intf, menu, qtr( "Stop" ),
            ":/stop", SLOT( stop() ) );
    addMIMStaticEntry( p_intf, menu, qtr( "Previous" ),
            ":/previous", SLOT( prev() ) );
    addMIMStaticEntry( p_intf, menu, qtr( "Next" ),
            ":/next", SLOT( next() ) );
}

void QVLCMenu::PopupMenuStaticEntries( QMenu *menu )
{
    QMenu *openmenu = new QMenu( qtr( "Open Media" ), menu );
    addDPStaticEntry( openmenu, qtr( "&Open File..." ),
        ":/file-asym", SLOT( openFileDialog() ) );
    addDPStaticEntry( openmenu, qtr( I_OPEN_FOLDER ),
        ":/folder-grey", SLOT( PLOpenDir() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Disc..." ),
        ":/disc", SLOT( openDiscDialog() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Network..." ),
        ":/network", SLOT( openNetDialog() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Capture Device..." ),
        ":/capture-card", SLOT( openCaptureDialog() ) );
    menu->addMenu( openmenu );

    menu->addSeparator();
#if 0
    QMenu *helpmenu = HelpMenu( menu );
    helpmenu->setTitle( qtr( "Help" ) );
    menu->addMenu( helpmenu );
#endif

    addDPStaticEntry( menu, qtr( "Quit" ), ":/quit",
                      SLOT( quit() ), "Ctrl+Q" );
}

/* Video Tracks and Subtitles tracks */
void QVLCMenu::VideoPopupMenu( intf_thread_t *p_intf )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_hold( p_input );
        vout_thread_t *p_vout = THEMIM->getVout();
        if( p_vout )
        {
            VideoAutoMenuBuilder( p_vout, p_input, objects, varnames );
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input );
    }
    QMenu *menu = new QMenu();
    CREATE_POPUP;
}

/* Audio Tracks */
void QVLCMenu::AudioPopupMenu( intf_thread_t *p_intf )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_hold( p_input );
        aout_instance_t *p_aout = THEMIM->getAout();
        AudioAutoMenuBuilder( p_aout, p_input, objects, varnames );
        if( p_aout )
            vlc_object_release( p_aout );
        vlc_object_release( p_input );
    }
    QMenu *menu = new QMenu();
    CREATE_POPUP;
}

/* Navigation stuff, and general menus ( open ) */
void QVLCMenu::MiscPopupMenu( intf_thread_t *p_intf )
{
    POPUP_BOILERPLATE;

    if( p_input )
    {
        vlc_object_hold( p_input );
        varnames.push_back( "audio-es" );
        InputAutoMenuBuilder( p_input, objects, varnames );
        PUSH_SEPARATOR;
    }

    QMenu *menu = new QMenu();
    Populate( p_intf, menu, varnames, objects );

    menu->addSeparator();
    PopupMenuControlEntries( menu, p_intf, p_input );

    menu->addSeparator();
    PopupMenuStaticEntries( menu );

    p_intf->p_sys->p_popup_menu = menu;
    menu->popup( QCursor::pos() );
    p_intf->p_sys->p_popup_menu = NULL;
}

/* Main Menu that sticks everything together  */
void QVLCMenu::PopupMenu( intf_thread_t *p_intf, bool show )
{
    MainInterface *mi = p_intf->p_sys->p_mi;
    if( show )
    {
        /* Delete and recreate a popup if there is one */
        if( p_intf->p_sys->p_popup_menu )
            delete p_intf->p_sys->p_popup_menu;

        QMenu *menu = new QMenu();
        QMenu *submenu;
        QAction *action;
        bool b_isFullscreen = false;

        POPUP_BOILERPLATE;

        PopupMenuControlEntries( menu, p_intf, p_input );
        menu->addSeparator();

        if( p_input )
        {
            vout_thread_t *p_vout = THEMIM->getVout();

            /* Add a fullscreen switch button */
            if( p_vout )
            {
                vlc_value_t val;
                var_Get( p_vout, "fullscreen", &val );
                b_isFullscreen = !( !val.b_bool );
                if( b_isFullscreen )
                    CreateAndConnect( menu, "fullscreen",
                            qtr( "Leave Fullscreen" ),"" , ITEM_NORMAL,
                            VLC_OBJECT(p_vout), val, VLC_VAR_BOOL,
                            b_isFullscreen );
                vlc_object_release( p_vout );
            }

            menu->addSeparator();

            /* Input menu */
            vlc_object_hold( p_input );
            InputAutoMenuBuilder( p_input, objects, varnames );
            vlc_object_release( p_input );

            /* Audio menu */
            submenu = new QMenu( menu );
            action = menu->addMenu( AudioMenu( p_intf, submenu ) );
            action->setText( qtr( "&Audio" ) );
            if( action->menu()->isEmpty() )
                action->setEnabled( false );

            /* Video menu */
            submenu = new QMenu( menu );
            action = menu->addMenu( VideoMenu( p_intf, submenu ) );
            action->setText( qtr( "&Video" ) );
            if( action->menu()->isEmpty() )
                action->setEnabled( false );

            /* Playback menu for chapters */
            submenu = new QMenu( menu );
            action = menu->addMenu( NavigMenu( p_intf, submenu ) );
            action->setText( qtr( "&Playback" ) );
            if( action->menu()->isEmpty() )
                action->setEnabled( false );
        }

        menu->addSeparator();

        /* Add some special entries for windowed mode: Interface Menu */
        if( !b_isFullscreen )
        {
            submenu = new QMenu( qtr( "Interface" ), menu );
            if( mi )
            {
                submenu->addAction( QIcon( ":/playlist" ),
                         qtr( "Show Playlist" ), mi, SLOT( togglePlaylist() ) );
                action = submenu->addAction( QIcon( "" ),
                     qtr( "Minimal View" ), mi, SLOT( toggleMinimalView() ) );
                action->setCheckable( true );
                action->setChecked( !( mi->getControlsVisibilityStatus() &
                            CONTROLS_VISIBLE ) );
                action = submenu->addAction( QIcon( "" ),
                        qtr( "Fullscreen Interface" ),
                        mi, SLOT( toggleFullScreen() ) );
                action->setCheckable( true );
                action->setChecked( mi->isFullScreen() );
            }
            else /* We are using the skins interface.
                    If not, this entry will not show. */
            {

                QMenu *tools = ToolsMenu( submenu );
                submenu->addSeparator();
                objects.clear();
                varnames.clear();
                vlc_object_t *p_object = ( vlc_object_t* )
                     vlc_object_find_name( p_intf, "skins2", FIND_PARENT );
                if( p_object )
                {
                    objects.push_back( p_object );
                    varnames.push_back( "intf-skins" );
                    Populate( p_intf, submenu, varnames, objects );
                    vlc_object_release( p_object );
                }
                else
                    msg_Dbg( p_intf, "could not find parent interface" );
            }
            menu->addMenu( submenu );
        }

        /* Static entries for ending, like open */
        PopupMenuStaticEntries( menu );

        p_intf->p_sys->p_popup_menu = menu;
        p_intf->p_sys->p_popup_menu->popup( QCursor::pos() );
    }
    else
    {
        // destroy popup if there is one
        delete p_intf->p_sys->p_popup_menu;
        p_intf->p_sys->p_popup_menu = NULL;
    }
}

#undef ACT_ADD
#undef ACT_ADDMENU
#undef ACT_ADDCHECK

/************************************************************************
 * Systray Menu                                                         *
 ************************************************************************/

void QVLCMenu::updateSystrayMenu( MainInterface *mi,
                                  intf_thread_t *p_intf,
                                  bool b_force_visible )
{
    POPUP_BOILERPLATE;

    /* Get the systray menu and clean it */
    QMenu *sysMenu = mi->getSysTrayMenu();
    sysMenu->clear();

    /* Hide / Show VLC and cone */
    if( mi->isVisible() || b_force_visible )
    {
        sysMenu->addAction( QIcon( ":/vlc16.png" ),
                            qtr( "Hide VLC media player in taskbar" ), mi,
                            SLOT( toggleUpdateSystrayMenu() ) );
    }
    else
    {
        sysMenu->addAction( QIcon( ":/vlc16.png" ),
                            qtr( "Show VLC media player" ), mi,
                            SLOT( toggleUpdateSystrayMenu() ) );
    }

    sysMenu->addSeparator();
    PopupMenuControlEntries( sysMenu, p_intf, p_input );

    sysMenu->addSeparator();
    addDPStaticEntry( sysMenu, qtr( "&Open Media" ),
            ":/file-wide", SLOT( openFileDialog() ) );
    addDPStaticEntry( sysMenu, qtr( "&Quit" ) ,
            ":/quit", SLOT( quit() ) );

    /* Set the menu */
    mi->getSysTray()->setContextMenu( sysMenu );
}

#undef PUSH_VAR
#undef PUSH_SEPARATOR

/*************************************************************************
 * Builders for automenus
 *************************************************************************/
QMenu * QVLCMenu::Populate( intf_thread_t *p_intf,
                            QMenu *current,
                            vector< const char *> & varnames,
                            vector<vlc_object_t *> & objects )
{
    QMenu *menu = current;
    if( !menu )
    {
        msg_Warn( p_intf,  "%s leaking a menu", __func__ );
        menu = new QMenu();
    }

    currentGroup = NULL;

    vlc_object_t *p_object;

    for( int i = 0; i < ( int )objects.size() ; i++ )
    {
        if( !varnames[i] || !*varnames[i] )
        {
            menu->addSeparator();
            continue;
        }
        p_object = objects[i];

        UpdateItem( p_intf, menu, varnames[i], p_object, true );
    }
    return menu;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

static bool IsMenuEmpty( const char *psz_var,
                         vlc_object_t *p_object,
                         bool b_root = true )
{
    vlc_value_t val, val_list;
    int i_type, i_result, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Check if we want to display the variable */
    if( !( i_type & VLC_VAR_HASCHOICE ) ) return false;

    var_Change( p_object, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int == 0 ) return true;

    if( ( i_type & VLC_VAR_TYPE ) != VLC_VAR_VARIABLE )
    {
        if( val.i_int == 1 && b_root ) return true;
        else return false;
    }

    /* Check children variables in case of VLC_VAR_VARIABLE */
    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST, &val_list, NULL ) < 0 )
    {
        return true;
    }

    for( i = 0, i_result = true; i < val_list.p_list->i_count; i++ )
    {
        if( !IsMenuEmpty( val_list.p_list->p_values[i].psz_string,
                    p_object, false ) )
        {
            i_result = false;
            break;
        }
    }

    /* clean up everything */
    var_Change( p_object, psz_var, VLC_VAR_FREELIST, &val_list, NULL );

    return i_result;
}

#define TEXT_OR_VAR qfu ( text.psz_string ? text.psz_string : psz_var )

void QVLCMenu::UpdateItem( intf_thread_t *p_intf, QMenu *menu,
        const char *psz_var, vlc_object_t *p_object, bool b_submenu )
{
    vlc_value_t val, text;
    int i_type;

    QAction *action = FindActionWithVar( menu, psz_var );
    if( action )
        DeleteNonStaticEntries( action->menu() );

    if( !p_object )
    {
        if( action )
            action->setEnabled( false );
        return;
    }

    /* Check the type of the object variable */
    /* What is the following HACK needed for? */
    if( !strcmp( psz_var, "audio-es" )
     || !strcmp( psz_var, "video-es" ) )
        i_type = VLC_VAR_INTEGER | VLC_VAR_HASCHOICE;
    else
        i_type = var_Type( p_object, psz_var );

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
        case VLC_VAR_FLOAT:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            if( action )
                action->setEnabled( false );
            return;
    }

    /* Make sure we want to display the variable */
    if( menu->isEmpty() && IsMenuEmpty( psz_var, p_object ) )
    {
        if( action )
            action->setEnabled( false );
        return;
    }

    /* Get the descriptive name of the variable */
    int i_ret = var_Change( p_object, psz_var, VLC_VAR_GETTEXT, &text, NULL );
    if( i_ret != VLC_SUCCESS )
    {
        text.psz_string = NULL;
    }

    if( !action )
    {
        action = new QAction( TEXT_OR_VAR, menu );
        menu->addAction( action );
        action->setData( psz_var );
    }

    /* Some specific stuff */
    bool forceDisabled = false;
    if( !strcmp( psz_var, "spu-es" ) )
    {
        vout_thread_t *p_vout = THEMIM->getVout();
        forceDisabled = ( p_vout == NULL );
        if( p_vout )
            vlc_object_release( p_vout );
    }

    if( i_type & VLC_VAR_HASCHOICE )
    {
        /* Append choices menu */
        if( b_submenu )
        {
            QMenu *submenu;
            submenu = action->menu();
            if( !submenu )
            {
                submenu = new QMenu( menu );
                action->setMenu( submenu );
            }

            action->setEnabled(
               CreateChoicesMenu( submenu, psz_var, p_object, true ) == 0 );
            if( forceDisabled )
                action->setEnabled( false );
        }
        else
        {
            action->setEnabled(
                CreateChoicesMenu( menu, psz_var, p_object, true ) == 0 );
        }
        FREENULL( text.psz_string );
        return;
    }

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
            var_Get( p_object, psz_var, &val );
            CreateAndConnect( menu, psz_var, TEXT_OR_VAR, "", ITEM_NORMAL,
                    p_object, val, i_type );
            break;

        case VLC_VAR_BOOL:
            var_Get( p_object, psz_var, &val );
            val.b_bool = !val.b_bool;
            CreateAndConnect( menu, psz_var, TEXT_OR_VAR, "", ITEM_CHECK,
                    p_object, val, i_type, !val.b_bool );
            break;
    }
    FREENULL( text.psz_string );
}


int QVLCMenu::CreateChoicesMenu( QMenu *submenu, const char *psz_var,
        vlc_object_t *p_object, bool b_root )
{
    vlc_value_t val, val_list, text_list;
    int i_type, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Make sure we want to display the variable */
    if( submenu->isEmpty() && IsMenuEmpty( psz_var, p_object, b_root ) )
        return VLC_EGENERIC;

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
        case VLC_VAR_FLOAT:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            return VLC_EGENERIC;
    }

    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        return VLC_EGENERIC;
    }

#define CURVAL val_list.p_list->p_values[i]
#define CURTEXT text_list.p_list->p_values[i].psz_string

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t another_val;
        QString menutext;
        QMenu *subsubmenu = new QMenu( submenu );

        switch( i_type & VLC_VAR_TYPE )
        {
            case VLC_VAR_VARIABLE:
                CreateChoicesMenu( subsubmenu, CURVAL.psz_string, p_object, false );
                subsubmenu->setTitle( qfu( CURTEXT ? CURTEXT :CURVAL.psz_string ) );
                submenu->addMenu( subsubmenu );
                break;

            case VLC_VAR_STRING:
                var_Get( p_object, psz_var, &val );
                another_val.psz_string = strdup( CURVAL.psz_string );
                menutext = qfu( CURTEXT ? CURTEXT : another_val.psz_string );
                CreateAndConnect( submenu, psz_var, menutext, "", ITEM_RADIO,
                        p_object, another_val, i_type,
                        val.psz_string && !strcmp( val.psz_string, CURVAL.psz_string ) );

                free( val.psz_string );
                break;

            case VLC_VAR_INTEGER:
                var_Get( p_object, psz_var, &val );
                if( CURTEXT ) menutext = qfu( CURTEXT );
                else menutext.sprintf( "%d", CURVAL.i_int );
                CreateAndConnect( submenu, psz_var, menutext, "", ITEM_RADIO,
                        p_object, CURVAL, i_type,
                        CURVAL.i_int == val.i_int );
                break;

            case VLC_VAR_FLOAT:
                var_Get( p_object, psz_var, &val );
                if( CURTEXT ) menutext = qfu( CURTEXT );
                else menutext.sprintf( "%.2f", CURVAL.f_float );
                CreateAndConnect( submenu, psz_var, menutext, "", ITEM_RADIO,
                        p_object, CURVAL, i_type,
                        CURVAL.f_float == val.f_float );
                break;

            default:
                break;
        }
    }
    currentGroup = NULL;

    /* clean up everything */
    var_Change( p_object, psz_var, VLC_VAR_FREELIST, &val_list, &text_list );

#undef CURVAL
#undef CURTEXT
    return submenu->isEmpty() ? VLC_EGENERIC : VLC_SUCCESS;
}

void QVLCMenu::CreateAndConnect( QMenu *menu, const char *psz_var,
        QString text, QString help,
        int i_item_type, vlc_object_t *p_obj,
        vlc_value_t val, int i_val_type,
        bool checked )
{
    QAction *action = FindActionWithVar( menu, psz_var );

    bool b_new = false;
    if( !action )
    {
        action = new QAction( text, menu );
        menu->addAction( action );
        b_new = true;
    }

    action->setToolTip( help );
    action->setEnabled( p_obj != NULL );

    if( i_item_type == ITEM_CHECK )
    {
        action->setCheckable( true );
    }
    else if( i_item_type == ITEM_RADIO )
    {
        action->setCheckable( true );
        if( !currentGroup )
            currentGroup = new QActionGroup( menu );
        currentGroup->addAction( action );
    }

    action->setChecked( checked );

    MenuItemData *itemData = new MenuItemData( THEDP->menusMapper, p_obj, i_val_type,
            val, psz_var );
    CONNECT( action, triggered(), THEDP->menusMapper, map() );
    THEDP->menusMapper->setMapping( action, itemData );

    if( b_new )
        menu->addAction( action );
}

void QVLCMenu::DoAction( QObject *data )
{
    MenuItemData *itemData = qobject_cast<MenuItemData *>( data );
    vlc_object_t *p_object = itemData->p_obj;
    if( p_object == NULL ) return;

    var_Set( p_object, itemData->psz_var, itemData->val );
}

void QVLCMenu::updateRecents( intf_thread_t *p_intf )
{
    if (recentsMenu)
    {
        QAction* action;
        RecentsMRL* rmrl = RecentsMRL::getInstance( p_intf );
        QList<QString> l = rmrl->recents();

        recentsMenu->clear();
        if( !l.size() )
        {
            action = recentsMenu->addAction( qtr(" - Empty - ") );
            action->setEnabled( false );
        }
        else
        {
            for( int i = 0; i < l.size(); ++i )
            {
                action = recentsMenu->addAction(
                        QString( "&%1: " ).arg( i + 1 ) + l.at( i ),
                        rmrl->signalMapper,
                        SLOT( map() ) );
                rmrl->signalMapper->setMapping( action, l.at( i ) );
            }

            recentsMenu->addSeparator();
            recentsMenu->addAction( qtr("&Clear"), rmrl, SLOT( clear() ) );
        }
    }
}
