/*
 * track_container.cpp - implementation of base-class for all track-containers
 *                       like Song-Editor, BB-Editor...
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

#include <Qt/QtXml>
#include <QApplication>
#include <QProgressDialog>
#include <QWheelEvent>

#else

#include <qdom.h>
#include <qapplication.h>
#include <qprogressdialog.h>

#define setValue setProgress
#define maximum totalSteps

#endif


#include "track_container.h"
#include "track.h"
#include "templates.h"
#include "bb_track.h"
#include "lmms_main_win.h"
#include "mixer.h"
#include "song_editor.h"
#include "string_pair_drag.h"
#include "channel_track.h"
#include "mmp.h"
#include "config_mgr.h"
#include "midi_file.h"



trackContainer::trackContainer() :
	QMainWindow( lmmsMainWin::inst()->workspace()
#ifndef QT4
				, 0, Qt::WStyle_Title
#endif
			 ),
	settings(),
	m_currentPosition( 0, 0 ),
	m_ppt( DEFAULT_PIXELS_PER_TACT )
{
#ifdef QT4
	if( lmmsMainWin::inst()->workspace() != NULL )
	{
		lmmsMainWin::inst()->workspace()->addWindow( this );
	}
#endif

	m_scrollArea = new scrollArea( this );
	m_scrollArea->show();

	setAcceptDrops( TRUE );
}




trackContainer::~trackContainer()
{
	while( m_trackWidgets.size() )
	{
		removeTrack( m_trackWidgets.front()->getTrack() );
	}
}




void trackContainer::saveSettings( QDomDocument & _doc, QDomElement & _parent )
{
	QDomElement tc_de = _doc.createElement( "trackcontainer" );
	tc_de.setAttribute( "type", nodeName() );
	_parent.appendChild( tc_de );

	// save settings of each track
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		( *it )->getTrack()->saveSettings( _doc, tc_de );
	}
}




void trackContainer::loadSettings( const QDomElement & _this )
{
	static QProgressDialog * pd = NULL;
	bool was_null = ( pd == NULL );
	int start_val = 0;
	if( pd == NULL )
	{
#ifdef QT4
		pd = new QProgressDialog( tr( "Loading project..." ),
						tr( "Cancel" ), 0,
						_this.childNodes().count() );
#else
		pd = new QProgressDialog( tr( "Loading project..." ),
						tr( "Cancel" ),
						_this.childNodes().count(),
								0, 0, TRUE );
#endif
		pd->setWindowTitle( tr( "Please wait..." ) );
		pd->show();
	}
	else
	{
#ifdef QT4
		start_val = pd->value();
		pd->setMaximum( pd->maximum() + _this.childNodes().count() );
#else
		start_val = pd->progress();
		pd->setTotalSteps( pd->maximum() + _this.childNodes().count() );
#endif
	}

	QDomNode node = _this.firstChild();
	while( !node.isNull() )
	{
#ifdef QT4
		pd->setValue( pd->value() + 1 );
		qApp->processEvents( QEventLoop::AllEvents, 100 );
#else
		pd->setValue( pd->progress() + 1 );
		qApp->processEvents( 100 );
#endif

		if( pd->wasCanceled() )
		{
			break;
		}

		if( node.isElement() )
		{
			track::create( node.toElement(), this );
		}
		node = node.nextSibling();
	}

	pd->setValue( start_val + _this.childNodes().count() );

	if( was_null )
	{
		delete pd;
		pd = NULL;
	}
}




void trackContainer::cloneTrack( track * _track )
{
	track::clone( _track );
}




void trackContainer::addTrack( track * _track )
{
	m_trackWidgets.push_back( _track->getTrackWidget() );
#ifndef QT4
	m_scrollArea->addChild( _track->getTrackWidget() );
#endif
	connect( this, SIGNAL( positionChanged( const midiTime & ) ),
				_track->getTrackWidget(),
				SLOT( changePosition( const midiTime & ) ) );
	realignTracks();
}




void trackContainer::removeTrack( track * _track )
{
	trackWidgetVector::iterator it = qFind( m_trackWidgets.begin(),
			m_trackWidgets.end(), _track->getTrackWidget() );
	if( it != m_trackWidgets.end() )
	{
		mixer::inst()->pause();
#ifndef QT4
		m_scrollArea->removeChild( _track->getTrackWidget() );
#endif
		m_trackWidgets.erase( it );

		delete _track;

		mixer::inst()->play();

		realignTracks();
		songEditor::inst()->setModified();
	}
}




void trackContainer::moveTrackUp( track * _track )
{
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		if( *it == _track->getTrackWidget() &&
						it > m_trackWidgets.begin() )
		{
			bbTrack::swapBBTracks( ( *it )->getTrack(),
						( *( it - 1 ) )->getTrack() );
			qSwap( *it, *( it - 1 ) );
			realignTracks();
			break;
		}
	}
}




void trackContainer::moveTrackDown( track * _track )
{
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		if( *it == _track->getTrackWidget() &&
						it + 1 < m_trackWidgets.end() )
		{
			bbTrack::swapBBTracks( ( *it )->getTrack(),
						( *( it + 1 ) )->getTrack() );
			qSwap( *it, *( it + 1 ) );
			realignTracks();
			break;
		}
	}
}




void trackContainer::updateAfterTrackAdd( void )
{
}




void trackContainer::realignTracks( bool _complete_update )
{
	int y = 0;
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		if( _complete_update )
		{
			( *it )->hide();
		}
		( *it )->show();
#ifdef QT4
		( *it )->move( 0, y );
#else
		m_scrollArea->moveChild( *it, 0, y );
#endif
		( *it )->resize( width(), ( *it )->height() );
		( *it )->changePosition( m_currentPosition );
		y += ( *it )->height();
	}
#ifndef QT4
	m_scrollArea->resizeContents( m_scrollArea->parentWidget()->width(),
									y );
#endif
	updateScrollArea();
}




unsigned int trackContainer::countTracks( track::trackTypes _tt ) const
{
	unsigned int cnt = 0;
	for( trackWidgetVector::const_iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		if( ( *it )->getTrack()->type() == _tt ||
					_tt == track::TOTAL_TRACK_TYPES )
		{
			++cnt;
		}
	}
	return( cnt );
}




void trackContainer::setMutedOfAllTracks( bool _muted )
{
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		( *it )->setMuted( _muted );
	}
}




constTrackVector trackContainer::tracks( void ) const
{
	constTrackVector tracks;
	for( trackWidgetVector::const_iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		tracks.push_back( ( *it )->getTrack() );
	}
	return( tracks );
}




trackVector trackContainer::tracks( void )
{
	trackVector tracks;
	for( trackWidgetVector::iterator it = m_trackWidgets.begin();
					it != m_trackWidgets.end(); ++it )
	{
		tracks.push_back( ( *it )->getTrack() );
	}
	return( tracks );
}




void trackContainer::setPixelsPerTact( Uint16 _ppt )
{
	m_ppt = _ppt;
}




void trackContainer::resizeEvent( QResizeEvent * )
{
	realignTracks();
}




void trackContainer::dragEnterEvent( QDragEnterEvent * _dee )
{
	stringPairDrag::processDragEnterEvent( _dee,
		QString( "presetfile,instrument,midifile,track_%1,track_%2" ).
						arg( track::CHANNEL_TRACK ).
						arg( track::SAMPLE_TRACK ) );
}




void trackContainer::dropEvent( QDropEvent * _de )
{
	QString type = stringPairDrag::decodeKey( _de );
	QString value = stringPairDrag::decodeValue( _de );
	if( type == "instrument" )
	{
		channelTrack * ct = dynamic_cast<channelTrack *>(
				track::create( track::CHANNEL_TRACK,
								this ) );
		ct->loadInstrument( value );
		ct->toggledChannelButton( TRUE );
		_de->accept();
	}
	else if( type == "presetfile" )
	{
		multimediaProject mmp( value );
		channelTrack * ct = dynamic_cast<channelTrack *>(
				track::create( track::CHANNEL_TRACK,
								this ) );
		ct->loadTrackSpecificSettings( mmp.content().firstChild().
								toElement() );
		ct->toggledChannelButton( TRUE );
		_de->accept();
	}
	else if( type == "midifile" )
	{
		midiFile mf( value );
		mf.importToTrackContainer( this );
		_de->accept();
	}
	else if( type.left( 6 ) == "track_" )
	{
		multimediaProject mmp( value, FALSE );
		track::create( mmp.content().firstChild().toElement(), this );
		// after adding a track, we have to make sure, actual editor
		// can setup new track (e.g. adding TCO's (bbEditor does so))
		updateAfterTrackAdd();
		_de->accept();
	}
}




void trackContainer::updateScrollArea( void )
{
	m_scrollArea->resize( tMax( m_scrollArea->parentWidget()->width() - 
					m_scrollArea->x() - 2, 0 ),
				tMax( m_scrollArea->parentWidget()->height() -
					m_scrollArea->y() - 2, 0 ) );
	//m_scrollArea->updateContents();
}




trackContainer::scrollArea::scrollArea( trackContainer * _parent ) :
	QScrollArea( _parent ),
	m_trackContainer( _parent )
{
	setFrameStyle( QFrame::NoFrame );
	setHorizontalScrollBarPolicy( 
#ifdef QT4
					Qt::ScrollBarAlwaysOff
#else
					QScrollArea::AlwaysOff
#endif
					);
}




trackContainer::scrollArea::~scrollArea()
{
}




void trackContainer::scrollArea::wheelEvent( QWheelEvent * _we )
{
	// always pass wheel-event to parent-widget (song-editor
	// bb-editor etc.) because they might want to use it for zooming
	// or scrolling left/right if a modifier-key is pressed, otherwise
	// they do not accept it and we pass it up to QScrollArea
	m_trackContainer->wheelEvent( _we );
	if( !_we->isAccepted() )
	{
		QScrollArea::wheelEvent( _we );
	}
}




#include "track_container.moc"

#undef setValue
#undef maximum
