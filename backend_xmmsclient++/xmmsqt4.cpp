/**
 *  This file is a part of Esperanza, an XMMS2 Client.
 *
 *  Copyright (C) 2005-2016 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient.h>
#include "xmmsqt4.h"

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>
#include <QMetaObject>
#include <QThread>
#include <QCoreApplication>

void XmmsQT4::applyPendingWriteState()
{
	if (m_hasPendingWriteState && m_initialized && m_wsock) {
		m_wsock->setEnabled(m_pendingWriteState);
		m_hasPendingWriteState = false;
	}
}

static void CheckWrite (int i, void *userdata);

XmmsQT4::XmmsQT4 (xmmsc_connection_t *xmmsc) :
	QObject (), Xmms::MainloopInterface (xmmsc),
	m_fd (0), m_rsock (0), m_wsock (0), m_xmmsc (xmmsc), m_initialized (false),
	m_hasPendingWriteState (false), m_pendingWriteState (false)
{
	m_fd = xmmsc_io_fd_get (xmmsc);
	xmmsc_io_need_out_callback_set (xmmsc, CheckWrite, this);

	QCoreApplication *app = QCoreApplication::instance();
	if (app && QThread::currentThread() == app->thread()) {
		// In main thread: initialize sockets.
		initializeSocketNotifiers();
	} else if (app) {
		// Off main thread.
		this->moveToThread(app->thread());
		QMetaObject::invokeMethod(this, "initializeSocketNotifiers",
		                         Qt::QueuedConnection);
	} else {
		// This should only happen in Qt4, in that case initialise directly.
		initializeSocketNotifiers();
	}

	running_ = true;
}

XmmsQT4::~XmmsQT4 ()
{
	if (m_rsock) {
		delete m_rsock;
	}
	if (m_wsock) {
		delete m_wsock;
	}
}

void XmmsQT4::initializeSocketNotifiers()
{
	if (m_initialized) {
		return;
	}

	m_rsock = new QSocketNotifier (m_fd, QSocketNotifier::Read, this);
	connect (m_rsock, SIGNAL (activated (int)), SLOT (OnRead ()));
	m_rsock->setEnabled (true);

	m_wsock = new QSocketNotifier (m_fd, QSocketNotifier::Write, this);
	connect (m_wsock, SIGNAL (activated (int)), SLOT (OnWrite ()));
	m_wsock->setEnabled (false);

	if (m_hasPendingWriteState) {
		m_wsock->setEnabled(m_pendingWriteState);
		m_hasPendingWriteState = false;
	}

	m_initialized = true;
}

void XmmsQT4::run ()
{
}

xmmsc_connection_t *XmmsQT4::GetXmmsConnection ()
{
	return m_xmmsc;
}


void XmmsQT4::OnRead ()
{
	if (!xmmsc_io_in_handle (m_xmmsc)) {
		return; /* exception? */
	}
}


void XmmsQT4::OnWrite ()
{
	if (!xmmsc_io_out_handle (m_xmmsc)) {
		return; /* exception? */
	}
}

void XmmsQT4::ToggleWrite (bool toggle)
{
	if (QThread::currentThread() != this->thread()) {
		m_pendingWriteState = toggle;
		m_hasPendingWriteState = true;
		QMetaObject::invokeMethod(this, "applyPendingWriteState",
		                         Qt::QueuedConnection);
		return;
	}

	if (!m_initialized || !m_wsock) {
		m_pendingWriteState = toggle;
		m_hasPendingWriteState = true;
		return;
	}

	m_wsock->setEnabled (toggle);
}

static void CheckWrite (int i, void *userdata)
{
	XmmsQT4 *obj = static_cast< XmmsQT4* > (userdata);

	if (xmmsc_io_want_out (obj->GetXmmsConnection ())) {
		obj->ToggleWrite (true);
	} else {
		obj->ToggleWrite (false);
	}
}

#include "xmmsqt4.moc"
