/*
 * track.cpp - implementation of classes concerning tracks -> neccessary for
 *             all track-like objects (beat/bassline, sample-track...)
 *
 * Copyright (c) 2004-2005 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#include "qt3support.h"

#ifdef QT4

#include <QApplication>
#include <Qt/QtXml>
#include <QMouseEvent>
#include <QMenu>
#include <QLayout>

#else

#include <qapplication.h>
#include <qdom.h>
#include <qpopupmenu.h>
#include <qlayout.h>
#include <qcursor.h>
#include <qwhatsthis.h>

#endif


#include "track.h"
#include "track_container.h"
#include "channel_track.h"
#include "bb_track.h"
#include "sample_track.h"
#include "song_editor.h"
#include "templates.h"
#include "clipboard.h"
#include "embed.h"
#include "pixmap_button.h"
#include "debug.h"
#include "tooltip.h"
#include "string_pair_drag.h"
#include "mmp.h"
#include "lmms_main_win.h"
#include "text_float.h"



const Sint16 RESIZE_GRIP_WIDTH = 4;

const Uint16 TRACK_OP_BTN_WIDTH = 20;
const Uint16 TRACK_OP_BTN_HEIGHT = 14;



textFloat * trackContentObject::s_textFloat = NULL;


// ===========================================================================
// trackContentObject
// ===========================================================================
trackContentObject::trackContentObject( track * _track ) :
	QWidget( _track->getTrackContentWidget()
#ifndef QT4
		, NULL, Qt::WDestructiveClose
#endif
 		),
	m_track( _track ),
	m_startPosition(),
	m_length(),
	m_moving( FALSE ),
	m_resizing( FALSE ),
	m_autoResize( FALSE ),
	m_initialMouseX( 0 )
{
	if( s_textFloat == NULL )
	{
		s_textFloat = new textFloat( this );
		s_textFloat->setPixmap( embed::getIconPixmap(
						"xclock", 24, 24 ) );
	}

#ifdef QT4
	setAttribute( Qt::WA_DeleteOnClose );
	setFocusPolicy( Qt::StrongFocus );
#else
	setFocusPolicy( StrongFocus );
#endif
	show();
	movePosition( 0 );
	changeLength( 0 );

	setFixedHeight( parentWidget()->height() - 2 );
	setAcceptDrops( TRUE );
	setMouseTracking( TRUE );
}




trackContentObject::~trackContentObject()
{
}




bool trackContentObject::fixedTCOs( void )
{
	return( m_track->getTrackContainer()->fixedTCOs() );
}




void trackContentObject::movePosition( const midiTime & _pos )
{
	if( m_startPosition != _pos )
	{
		songEditor::inst()->setModified();
	}
	m_startPosition = _pos;
	m_track->getTrackWidget()->changePosition();
	// moving of TCO can result in change of song-length etc.,
	// therefore we update the trackcontainer
	m_track->getTrackContainer()->update();
}




void trackContentObject::changeLength( const midiTime & _length )
{
	if( m_length != _length )
	{
		songEditor::inst()->setModified();
	}
	m_length = _length;
	setFixedWidth( static_cast<int>( m_length * pixelsPerTact() / 64 ) +
							TCO_BORDER_WIDTH*2 );
	// changing length of TCO can result in change of song-length etc.,
	// therefore we update the trackcontainer
	m_track->getTrackContainer()->update();
}




float trackContentObject::pixelsPerTact( void )
{
	if( fixedTCOs() )
	{
		return( getTrack()->getTrackContentWidget()->width() -
				2 * TCO_BORDER_WIDTH ) /
				tMax<float>( length().getTact(), 1.0f );
	}
	return( getTrack()->getTrackContainer()->pixelsPerTact() );
}




void trackContentObject::dragEnterEvent( QDragEnterEvent * _dee )
{
	stringPairDrag::processDragEnterEvent( _dee, "tco_" +
					QString::number( m_track->type() ) );
}




void trackContentObject::dropEvent( QDropEvent * _de )
{
	QString type = stringPairDrag::decodeKey( _de );
	QString value = stringPairDrag::decodeValue( _de );
	if( type == ( "tco_" + QString::number( m_track->type() ) ) )
	{
		// value contains our XML-data so simply create a
		// multimediaProject which does the rest for us...
		multimediaProject mmp( value, FALSE );
		// at least save position before getting to moved to somewhere
		// the user doesn't expect...
		midiTime pos = startPosition();
		loadSettings( mmp.content().firstChild().toElement() );
		movePosition( pos );
		_de->accept();
	}
}




void trackContentObject::leaveEvent( QEvent * _e )
{
	while( QApplication::overrideCursor() != NULL )
	{
		QApplication::restoreOverrideCursor();
	}
	if( _e != NULL )
	{
		QWidget::leaveEvent( _e );
	}
}




void trackContentObject::mousePressEvent( QMouseEvent * _me )
{
	if( _me->button() == Qt::LeftButton && lmmsMainWin::isCtrlPressed() )
	{
		multimediaProject mmp( multimediaProject::DRAG_N_DROP_DATA );
		saveSettings( mmp, mmp.content() );
#ifdef QT4
		QPixmap thumbnail = QPixmap::grabWidget( this ).scaled(
						128, 128,
						Qt::KeepAspectRatio,
						Qt::SmoothTransformation );
#else
		QSize s( size() );
		s.scale( 128, 128, QSize::ScaleMin );
		QPixmap thumbnail = QPixmap::grabWidget( this ).
					convertToImage().smoothScale( s );
#endif
		new stringPairDrag( "tco_" +
					QString::number( m_track->type() ),
					mmp.toString(), thumbnail, this );
	}
	else if( _me->button() == Qt::LeftButton &&
			/*	lmmsMainWin::isShiftPressed() == FALSE &&*/
							fixedTCOs() == FALSE )
	{
		m_initialMouseX = _me->x();
		
		if( _me->x() < width() - RESIZE_GRIP_WIDTH )
		{
			m_moving = TRUE;
			QCursor c( Qt::SizeAllCursor );
			QApplication::setOverrideCursor( c );
			s_textFloat->setTitle( tr( "Current position" ) );
		}
		else if( m_autoResize == FALSE )
		{
			m_resizing = TRUE;
			QCursor c( Qt::SizeHorCursor );
			QApplication::setOverrideCursor( c );
			s_textFloat->setTitle( tr( "Current length" ) );
		}
		s_textFloat->reparent( this );
		// setup text-float as if TCO was already moved/resized
		mouseMoveEvent( _me );
		s_textFloat->show();
	}
	else if( ( _me->button() == Qt::MidButton/* ||
			( _me->button() == Qt::LeftButton &&
		  		lmmsMainWin::isShiftPressed() == TRUE )*/ ) &&
							fixedTCOs() == FALSE )
	{
		close();
	}
}




void trackContentObject::mouseMoveEvent( QMouseEvent * _me )
{
	const float ppt = m_track->getTrackContainer()->pixelsPerTact();
	if( m_moving )
	{
		int x = mapToParent( _me->pos() ).x() - m_initialMouseX;
		movePosition( tMax( 0, (Sint32) m_track->getTrackContainer()->
							currentPosition() +
					static_cast<int>( x * 64 / ppt ) ) );
		m_track->getTrackWidget()->changePosition();
		s_textFloat->setText( QString( "%1:%2" ).
					arg( m_startPosition.getTact() + 1 ).
					arg( m_startPosition.getTact64th() ) );
		s_textFloat->move( mapTo( topLevelWidget(), QPoint( 0, 0 ) ) +
				QPoint( -2 - s_textFloat->width(), 8 ) );
	}
	else if( m_resizing )
	{
		changeLength( tMax( 64,
				static_cast<int>( _me->x() * 64 / ppt ) ) );
		s_textFloat->setText( QString( "%1:%2" ).
					arg( length().getTact() + 1 ).
					arg( length().getTact64th() ) );
		s_textFloat->move( mapTo( topLevelWidget(), QPoint( 0, 0 ) ) +
						QPoint( width() + 2, 8 ) );
	}
	else
	{
		if( _me->x() > width() - RESIZE_GRIP_WIDTH )
		{
			if( QApplication::overrideCursor() != NULL &&
				QApplication::overrideCursor()->shape() !=
							Qt::SizeHorCursor )
			{
				while( QApplication::overrideCursor() != NULL )
				{
					QApplication::restoreOverrideCursor();
				}
			}
			QCursor c( Qt::SizeHorCursor );
			QApplication::setOverrideCursor( c );
		}
		else
		{
			leaveEvent( NULL );
		}
	}
}




void trackContentObject::mouseReleaseEvent( QMouseEvent * _me )
{
	s_textFloat->hide();
	leaveEvent( NULL );
	m_moving = FALSE;
	m_resizing = FALSE;
}




void trackContentObject::contextMenuEvent( QContextMenuEvent * _cme )
{
	QMenu contextMenu( this );
	if( fixedTCOs() == FALSE )
	{
		contextMenu.addAction( embed::getIconPixmap( "cancel" ),
					tr( "Delete (middle mousebutton)" ),
							this, SLOT( close() ) );
#ifdef QT4
		contextMenu.addSeparator();
#else
		contextMenu.insertSeparator();
#endif
		contextMenu.addAction( embed::getIconPixmap( "edit_cut" ),
					tr( "Cut" ), this, SLOT( cut() ) );
	}
	contextMenu.addAction( embed::getIconPixmap( "edit_copy" ),
					tr( "Copy" ), this, SLOT( copy() ) );
	contextMenu.addAction( embed::getIconPixmap( "edit_paste" ),
					tr( "Paste" ), this, SLOT( paste() ) );
	//contextMenu.insertSeparator();
	//contextMenu.insertItem( tr( "&Help" ), this, SLOT( displayHelp() ) );

	constructContextMenu( &contextMenu );

	contextMenu.exec( QCursor::pos() );
}




void trackContentObject::close( void )
{
	m_track->getTrackContentWidget()->removeTCO( this, FALSE );
	// we have to give our track-container the focus because otherwise the
	// op-buttons of our track-widgets could become focus and when the user
	// presses space for playing song, just one of these buttons is pressed
	// which results in unwanted effects
	m_track->getTrackContainer()->setFocus();
	QWidget::close();
}




void trackContentObject::cut( void )
{
	copy();
	close();
}




void trackContentObject::copy( void )
{
	clipboard::copy( this );
}




void trackContentObject::paste( void )
{
	if( clipboard::getContent( nodeName() ) != NULL )
	{
		loadSettings( *( clipboard::getContent( nodeName() ) ) );
	}
}




void trackContentObject::setAutoResizeEnabled( bool _e )
{
	m_autoResize = _e;
}




// ===========================================================================
// trackContentWidget
// ===========================================================================
trackContentWidget::trackContentWidget( trackWidget * _parent ) :
	QWidget( _parent ),
	m_trackWidget( _parent )
{
#ifdef QT4
	QPalette pal;
	pal.setColor( backgroundRole(), QColor( 96, 96, 96 ) );
	setPalette( pal );
#else
	setPaletteBackgroundColor( QColor( 96, 96, 96 ) );
#endif
	setMouseTracking( TRUE );
	setAcceptDrops( TRUE );
}




trackContentWidget::~trackContentWidget()
{
}




trackContentObject * FASTCALL trackContentWidget::getTCO( csize _tco_num )
{
	if( _tco_num < m_trackContentObjects.size() )
	{
		return( m_trackContentObjects[_tco_num] );
	}
	printf( "called trackContentWidget::getTCO( %d, TRUE ), "
			" but TCO %d doesn't exist\n", _tco_num, _tco_num );
	return( getTrack()->addTCO( getTrack()->createTCO( _tco_num * 64 ) ) );
//	return( NULL );
}




csize trackContentWidget::numOfTCOs( void )
{
	return( m_trackContentObjects.size() );
}




trackContentObject * FASTCALL trackContentWidget::addTCO(
						trackContentObject * _tco )
{
	m_trackContentObjects.push_back( _tco );
	_tco->move( 0, 1 );
	m_trackWidget->changePosition();
	songEditor::inst()->setModified();
	return( _tco );		// just for convenience
}




void FASTCALL trackContentWidget::removeTCO( csize _tco_num, bool _also_delete )
{
	removeTCO( getTCO( _tco_num ), _also_delete );
}




void trackContentWidget::removeTCO( trackContentObject * _tco,
							bool _also_delete )
{
	tcoVector::iterator it = qFind( m_trackContentObjects.begin(),
					m_trackContentObjects.end(),
					_tco );
	if( it != m_trackContentObjects.end() )
	{
		if( _also_delete )
		{
			delete _tco;
		}
		m_trackContentObjects.erase( it );
		songEditor::inst()->setModified();
	}
}




void trackContentWidget::removeAllTCOs( void )
{
	while( !m_trackContentObjects.empty() )
	{
		delete m_trackContentObjects.front();
		m_trackContentObjects.erase( m_trackContentObjects.begin() );
	}
}




void trackContentWidget::swapPositionOfTCOs( csize _tco_num1, csize _tco_num2 )
{
	// TODO: range-checking
	qSwap( m_trackContentObjects[_tco_num1],
					m_trackContentObjects[_tco_num2] );

	const midiTime pos = m_trackContentObjects[_tco_num1]->startPosition();

	m_trackContentObjects[_tco_num1]->movePosition(
			m_trackContentObjects[_tco_num2]->startPosition() );
	m_trackContentObjects[_tco_num2]->movePosition( pos );
}




tact trackContentWidget::length( void ) const
{
	// find last end-position
	midiTime last = 0;
	for( tcoVector::const_iterator it = m_trackContentObjects.begin();
				it != m_trackContentObjects.end(); ++it )
	{
		last = tMax( ( *it )->endPosition(), last );
	}
	return( last.getTact() + ( ( last.getTact64th() != 0 )? 1 : 0 ) );
}




void trackContentWidget::insertTact( const midiTime & _pos )
{
	// we'll increase the position of every TCO, posated behind _pos, by
	// one tact
	for( tcoVector::iterator it = m_trackContentObjects.begin();
				it != m_trackContentObjects.end(); ++it )
	{
		if( ( *it )->startPosition() >= _pos )
		{
			( *it )->movePosition( (*it)->startPosition() + 64 );
		}
	}
}




void trackContentWidget::removeTact( const midiTime & _pos )
{
	// we'll decrease the position of every TCO, posated behind _pos, by
	// one tact
	for( tcoVector::iterator it = m_trackContentObjects.begin();
				it != m_trackContentObjects.end(); ++it )
	{
		if( ( *it )->startPosition() >= _pos )
		{
			( *it )->movePosition( tMax( ( *it )->startPosition() -
								    64, 0 ) );
		}
	}
}




void trackContentWidget::updateTCOs( void )
{
	for( tcoVector::iterator it = m_trackContentObjects.begin();
				it != m_trackContentObjects.end(); ++it )
	{
		( *it )->update();
	}
}




void trackContentWidget::dragEnterEvent( QDragEnterEvent * _dee )
{
	stringPairDrag::processDragEnterEvent( _dee, "tco_" +
					QString::number( getTrack()->type() ) );
}




void trackContentWidget::dropEvent( QDropEvent * _de )
{
	QString type = stringPairDrag::decodeKey( _de );
	QString value = stringPairDrag::decodeValue( _de );
	if( type == ( "tco_" + QString::number( getTrack()->type() ) ) &&
			getTrack()->getTrackContainer()->fixedTCOs() == FALSE )
	{
		const midiTime position = getPosition( _de->pos().x() );
		trackContentObject * tco = addTCO( getTrack()->createTCO(
								position ) );
		// value contains our XML-data so simply create a
		// multimediaProject which does the rest for us...
		multimediaProject mmp( value, FALSE );
		// at least save position before getting to moved to somewhere
		// the user doesn't expect...
		tco->loadSettings( mmp.content().firstChild().toElement() );
		tco->movePosition( position );
		_de->accept();
	}
}






void trackContentWidget::mousePressEvent( QMouseEvent * _me )
{
	if( _me->button() == Qt::LeftButton &&
			getTrack()->getTrackContainer()->fixedTCOs() == FALSE )
	{
		const midiTime position = getPosition( _me->x() );
		trackContentObject * tco = addTCO( getTrack()->createTCO(
								position ) );
		tco->movePosition( position );
	}
}




void trackContentWidget::mouseMoveEvent( QMouseEvent * _me )
{
	// if user moved TCO out of visible area, TCO doesn't receive
	// mouse-events, so we have to do it here
	while( QApplication::overrideCursor() != NULL )
	{
		QApplication::restoreOverrideCursor();
	}
}




void trackContentWidget::mouseReleaseEvent( QMouseEvent * _me )
{
	// if user moved TCO out of visible area, TCO doesn't receive
	// mouseRelease-events...
	while( QApplication::overrideCursor() != NULL )
	{
		QApplication::restoreOverrideCursor();
	}
}




void trackContentWidget::resizeEvent( QResizeEvent * _re )
{
	updateTCOs();
}




track * trackContentWidget::getTrack( void )
{
	return( m_trackWidget->getTrack() );
}




midiTime trackContentWidget::getPosition( int _mouse_x )
{
	const trackContainer * tc = getTrack()->getTrackContainer();
	return( midiTime( tc->currentPosition() + _mouse_x * 64 /
				static_cast<int>( tc->pixelsPerTact() ) ) );
}




// ===========================================================================
// trackSettingsWidget
// ===========================================================================

trackSettingsWidget::trackSettingsWidget( trackWidget * _parent ) :
	QWidget( _parent ),
	m_trackWidget( _parent )
{
}




trackSettingsWidget::~trackSettingsWidget()
{
}




void trackSettingsWidget::mousePressEvent( QMouseEvent * _me )
{
	if( _me->button() == Qt::LeftButton &&
			m_trackWidget->getTrack()->type() != track::BB_TRACK )
	{
		multimediaProject mmp( multimediaProject::DRAG_N_DROP_DATA );
		m_trackWidget->getTrack()->saveSettings( mmp, mmp.content() );
		new stringPairDrag( "track_" +
			QString::number( m_trackWidget->getTrack()->type() ),
			mmp.toString(), QPixmap::grabWidget( this ), this );
	}
}






// ===========================================================================
// trackWidget
// ===========================================================================

trackWidget::trackWidget( track * _track, QWidget * _parent ) :
	QWidget( _parent ),
	m_track( _track ),
	m_trackOperationsWidget( this ),
	m_trackSettingsWidget( this ),
	m_trackContentWidget( this )
{
#ifdef QT4
	{
		QPalette pal;
		pal.setColor( m_trackOperationsWidget.backgroundRole(),
						QColor( 128, 128, 128 ) );
		m_trackOperationsWidget.setPalette( pal );
	}
#else
	m_trackOperationsWidget.setPaletteBackgroundColor(
						QColor( 128, 128, 128 ) );
#endif


	QPushButton * clntr_btn = new QPushButton( embed::getIconPixmap(
						"edit_copy", 12, 12 ),
									"",
						&m_trackOperationsWidget );
	clntr_btn->setGeometry( 1, 1, TRACK_OP_BTN_WIDTH, TRACK_OP_BTN_HEIGHT );
	connect( clntr_btn, SIGNAL( clicked() ), this, SLOT( cloneTrack() ) );
	connect( clntr_btn, SIGNAL( clicked() ), clntr_btn,
							SLOT( clearFocus() ) );
	toolTip::add( clntr_btn, tr( "Clone this track" ) );

	QPushButton * deltr_btn = new QPushButton( embed::getIconPixmap(
							"cancel", 12, 12 ),
						"", &m_trackOperationsWidget );
	deltr_btn->setGeometry( 1, 1+TRACK_OP_BTN_HEIGHT, TRACK_OP_BTN_WIDTH,
							TRACK_OP_BTN_HEIGHT );
	connect( deltr_btn, SIGNAL( clicked() ), this, SLOT( removeTrack() ) );
	connect( deltr_btn, SIGNAL( clicked() ), deltr_btn,
							SLOT( clearFocus() ) );
	toolTip::add( deltr_btn, tr( "Remove this track" ) );

	QPushButton * muptr_btn = new QPushButton( embed::getIconPixmap(
							"arp_up_on", 12, 12 ),
						"", &m_trackOperationsWidget );
	muptr_btn->setGeometry( 1+TRACK_OP_BTN_WIDTH, 1, TRACK_OP_BTN_WIDTH,
							TRACK_OP_BTN_HEIGHT );
	connect( muptr_btn, SIGNAL( clicked() ), this, SLOT( moveTrackUp() ) );
	connect( muptr_btn, SIGNAL( clicked() ), muptr_btn,
							SLOT( clearFocus() ) );
	toolTip::add( muptr_btn, tr( "Move this track up" ) );

	QPushButton * mdowntr_btn = new QPushButton( embed::getIconPixmap(
							"arp_down_on", 12, 12 ),
						"", &m_trackOperationsWidget );
	mdowntr_btn->setGeometry( 1+TRACK_OP_BTN_WIDTH, 1+TRACK_OP_BTN_HEIGHT,
							TRACK_OP_BTN_WIDTH,
							TRACK_OP_BTN_HEIGHT );
	connect( mdowntr_btn, SIGNAL( clicked() ), this,
						SLOT( moveTrackDown() ) );
	connect( mdowntr_btn, SIGNAL( clicked() ), mdowntr_btn,
							SLOT( clearFocus() ) );
	toolTip::add( mdowntr_btn, tr( "Move this track down" ) );

	m_muteBtn = new pixmapButton( &m_trackOperationsWidget );
	m_muteBtn->setActiveGraphic( embed::getIconPixmap( "mute_on" ) );
	m_muteBtn->setInactiveGraphic( embed::getIconPixmap( "mute_off" ) );
	m_muteBtn->move( 3+TRACK_OP_BTN_WIDTH*2, 8 );
	m_muteBtn->show();
	connect( m_muteBtn, SIGNAL( toggled( bool ) ), this,
						SLOT( setMuted( bool ) ) );
	connect( m_muteBtn, SIGNAL( clickedRight() ), this,
					SLOT( muteBtnRightClicked() ) );
#ifdef QT4
	m_muteBtn->setWhatsThis(
#else
	QWhatsThis::add( m_muteBtn,
#endif
		tr( "With this switch you can either mute this track or mute "
			"all other tracks.\nBy clicking left, this track is "
			"muted. This is useful, if you only want to listen to "
			"the other tracks without changing this track "
			"and loosing information.\nWhen you click right on "
			"this switch, all other tracks will be "
			"muted. This is useful, if you only want to listen to "
			"this track." ) );
	toolTip::add( m_muteBtn, tr( "left click = mute this track\n"
			"right click = mute all other tracks (solo)" ) );


#ifdef QT4
	{
		QPalette pal;
		pal.setColor( m_trackSettingsWidget.backgroundRole(),
							QColor( 64, 64, 64 ) );
		m_trackSettingsWidget.setPalette( pal );
	}
#else
	m_trackSettingsWidget.setPaletteBackgroundColor( QColor( 64, 64, 64 ) );
#endif
	// set background-mode for flicker-free redraw
#ifndef QT4
	setBackgroundMode( Qt::NoBackground );
#endif
	setAcceptDrops( TRUE );
	setMouseTracking( TRUE );
}




trackWidget::~trackWidget()
{
}




bool trackWidget::muted( void ) const
{
#ifdef QT4
	return( m_muteBtn->isChecked() );
#else
	return( m_muteBtn->isOn() );
#endif
}




// resposible for moving track-content-widgets to appropriate position after
// change of visible viewport
void trackWidget::changePosition( const midiTime & _new_pos )
{
	midiTime pos = _new_pos;
	if( pos < 0 )
	{
		pos = m_track->getTrackContainer()->currentPosition();
	}

	const Sint32 begin = pos;
	const Sint32 end = endPosition( pos );
	const float ppt = m_track->getTrackContainer()->pixelsPerTact();
	const csize tcos = m_trackContentWidget.numOfTCOs();

	for( csize i = 0; i < tcos; ++i )
	{
		trackContentObject * tco =
					m_trackContentWidget.getTCO( i );
		tco->changeLength( tco->length() );
		Sint32 ts = tco->startPosition();
		Sint32 te = tco->endPosition();
		if( ( ts >= begin && ts <= end ) ||
			( te >= begin && te <= end ) ||
			( ts <= begin && te >= end ) )
		{
			tco->move( static_cast<int>( ( ts - begin ) * ppt /
							64 ), tco->y() );
			tco->show();
		}
		else
		{
			tco->hide();
		}
	}
}




void trackWidget::cloneTrack( void )
{
	m_track->getTrackContainer()->cloneTrack( m_track );
}




void trackWidget::removeTrack( void )
{
	m_track->getTrackContainer()->removeTrack( m_track );
}




void trackWidget::moveTrackUp( void )
{
	m_track->getTrackContainer()->moveTrackUp( m_track );
}




void trackWidget::moveTrackDown( void )
{
	m_track->getTrackContainer()->moveTrackDown( m_track );
}




void trackWidget::setMuted( bool _muted )
{
#ifdef QT4
	m_muteBtn->setChecked( _muted );
#else
	m_muteBtn->setOn( _muted );
#endif
	m_trackContentWidget.updateTCOs();
}




void trackWidget::muteBtnRightClicked( void )
{
	bool m = muted();
	m_track->getTrackContainer()->setMutedOfAllTracks( m );
	setMuted( !m );
}




void trackWidget::dragEnterEvent( QDragEnterEvent * _dee )
{
	stringPairDrag::processDragEnterEvent( _dee, "track_" +
					QString::number( m_track->type() ) );
}




void trackWidget::dropEvent( QDropEvent * _de )
{
	QString type = stringPairDrag::decodeKey( _de );
	QString value = stringPairDrag::decodeValue( _de );
	if( type == ( "track_" + QString::number( m_track->type() ) ) )
	{
		// value contains our XML-data so simply create a
		// multimediaProject which does the rest for us...
		multimediaProject mmp( value, FALSE );
		m_track->loadSettings( mmp.content().firstChild().toElement() );
		_de->accept();
	}
}




void trackWidget::paintEvent( QPaintEvent * _pe )
{
#ifdef QT4
	QPainter p( this );
#else
	// create pixmap for whole widget
	QPixmap pm( rect().size() );
	pm.fill( QColor( 128, 128, 128 ) );

	// and a painter for it
	QPainter p( &pm );
#endif
	p.setPen( QColor( 0, 0, 0 ) );
	p.drawLine( 0, height() - 1, width() - 1, height() - 1 );

#ifndef QT4
	// blit drawn pixmap to actual widget
	bitBlt( this, rect().topLeft(), &pm );
#endif
}




void trackWidget::resizeEvent( QResizeEvent * _re )
{
	m_trackOperationsWidget.setFixedSize( TRACK_OP_WIDTH, height() - 1 );
	m_trackOperationsWidget.move( 0, 0 );
	m_trackSettingsWidget.setFixedSize( DEFAULT_SETTINGS_WIDGET_WIDTH,
								height() - 1 );
	m_trackSettingsWidget.move( TRACK_OP_WIDTH, 0 );
	m_trackContentWidget.resize( m_track->getTrackContainer()->width() -
								TRACK_OP_WIDTH -
						DEFAULT_SETTINGS_WIDGET_WIDTH,
								height() - 1 );
	m_trackContentWidget.move( m_trackOperationsWidget.width() +
					m_trackSettingsWidget.width(), 0 );
}




midiTime trackWidget::endPosition( const midiTime & _pos_start )
{
	const float ppt = m_track->getTrackContainer()->pixelsPerTact();
	const int cww = m_trackContentWidget.width();
	return( _pos_start + static_cast<int>( cww * 64 / ppt ) );
}






// ===========================================================================
// track
// ===========================================================================

track::track( trackContainer * _tc ) :
	settings(),
	m_trackContainer( _tc )
{
	m_trackWidget = new trackWidget( this,
					m_trackContainer->containerWidget() );

	m_trackContainer->addTrack( this );
}




track::~track()
{
	m_trackContainer->removeTrack( this );

	delete m_trackWidget;
	m_trackWidget = NULL;
}




track * FASTCALL track::create( trackTypes _tt, trackContainer * _tc )
{
	switch( _tt )
	{
		case CHANNEL_TRACK: return( new channelTrack( _tc ) );
		case BB_TRACK: return( new bbTrack( _tc ) );
		case SAMPLE_TRACK: return( new sampleTrack( _tc ) );
//		case EVENT_TRACK:
//		case VIDEO_TRACK:
		default: break;
	}
	qFatal( "Attempt to create track with invalid type %d!",
						static_cast<int>( _tt ) );
	return( NULL );
}




track * FASTCALL track::create( const QDomElement & _this,
							trackContainer * _tc )
{
	track * t = create( static_cast<trackTypes>( _this.attribute(
						"type" ).toInt() ), _tc );
#ifdef LMMS_DEBUG
	assert( t != NULL );
#endif
	t->loadSettings( _this );
	return( t );
}




track * FASTCALL track::clone( track * _track )
{
	QDomDocument doc;
	QDomElement parent = doc.createElement( "clone" );
	_track->saveSettings( doc, parent );
	QDomElement e = parent.firstChild().toElement();
	return( create( e, _track->getTrackContainer() ) );
}




tact track::length( void ) const
{
	return( getTrackContentWidget()->length() );
}




void FASTCALL track::saveSettings( QDomDocument & _doc, QDomElement & _parent )
{
	csize num_of_tcos = getTrackContentWidget()->numOfTCOs();

	QDomElement track_de = _doc.createElement( "track" );
	track_de.setAttribute( "type", QString::number( type() ) );
	track_de.setAttribute( "muted", muted() );
	_parent.appendChild( track_de );

	// let actual track (channelTrack, bbTrack, sampleTrack etc.) save
	// its settings
	saveTrackSpecificSettings( _doc, track_de );

	// now save settings of all TCO's
	for( csize i = 0; i < num_of_tcos; ++i )
	{
		trackContentObject * tco = getTCO( i );
		tco->saveSettings( _doc, track_de );
	}
}




void FASTCALL track::loadSettings( const QDomElement & _this )
{
	if( _this.attribute( "type" ).toInt() != type() )
	{
		qWarning( "Current track-type does not match track-type of "
							"settings-node!\n" );
	}

	m_trackWidget->setMuted( _this.attribute( "muted" ).toInt() );


	getTrackContentWidget()->removeAllTCOs();

	QDomNode node = _this.firstChild();
	while( !node.isNull() )
	{
		if( node.isElement() )
		{
			if( nodeName() == node.nodeName() )
			{
				loadTrackSpecificSettings( node.toElement() );
			}
			else
			{
				trackContentObject * tco = createTCO(
								midiTime( 0 ) );
				tco->loadSettings( node.toElement() );
				addTCO( tco );
			}
		}
		node = node.nextSibling();
        }
}




trackContentObject * FASTCALL track::addTCO( trackContentObject * _tco )
{
	return( getTrackContentWidget()->addTCO( _tco ) );
}




void FASTCALL track::removeTCO( csize _tco_num )
{
	getTrackContentWidget()->removeTCO( _tco_num );
}




csize track::numOfTCOs( void )
{
	return( getTrackContentWidget()->numOfTCOs() );
}




trackContentObject * FASTCALL track::getTCO( csize _tco_num )
{
	return( getTrackContentWidget()->getTCO( _tco_num ) );
	
}




csize FASTCALL track::getTCONum( trackContentObject * _tco )
{
	for( csize i = 0; i < getTrackContentWidget()->numOfTCOs(); ++i )
	{
		if( getTCO( i ) == _tco )
		{
			return( i );
		}
	}
#ifdef LMMS_DEBUG
	qFatal( "track::getTCONum(...) -> _tco not found!\n" );
#endif
	return( 0 );
}




void FASTCALL track::getTCOsInRange( vlist<trackContentObject *> & _tco_v,
							const midiTime & _start,
							const midiTime & _end )
{
	for( csize i = 0; i < getTrackContentWidget()->numOfTCOs(); ++i )
	{
		trackContentObject * tco = getTCO( i );
		Sint32 s = tco->startPosition();
		Sint32 e = tco->endPosition();
		if( ( s <= _end ) && ( e >= _start ) )
		{
			// ok, TCO is posated within given range
			// now let's search according position for TCO in list
			// 	-> list is ordered by TCO's position afterwards
			bool inserted = FALSE;
			for( vlist<trackContentObject *>::iterator it =
								_tco_v.begin();
						it != _tco_v.end(); ++it )
			{
				if( ( *it )->startPosition() >= s )
				{
					_tco_v.insert( it, tco );
					inserted = TRUE;
					break;
				}
			}
			if( inserted == FALSE )
			{
				// no TCOs found posated behind current TCO...
				_tco_v.push_back( tco );
			}
		}
	}
}




void FASTCALL track::swapPositionOfTCOs( csize _tco_num1, csize _tco_num2 )
{
	getTrackContentWidget()->swapPositionOfTCOs( _tco_num1, _tco_num2 );
}



#include "track.moc"

