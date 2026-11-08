/*
 * tde-firewalld - firewalld manager for the Trinity Desktop Environment
 *
 * Copyright (C) 2026 tde-firewalld developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mainwindow.h"
#include "firewallwidget.h"

#include <tqlayout.h>

#include <tdeaccel.h>
#include <tdeaction.h>
#include <tdeapplication.h>
#include <kdialog.h>
#include <tdelocale.h>
#include <tdemenubar.h>
#include <tdepopupmenu.h>
#include <kstatusbar.h>
#include <kstdaction.h>

MainWindow::MainWindow()
	: TDEMainWindow(0, "tde-firewalld main window")
{
	TQWidget *central = new TQWidget(this);
	TQVBoxLayout *l = new TQVBoxLayout(central, KDialog::marginHint(), 0);
	m_widget = new FirewallWidget(central);
	m_widget->setMessageLineVisible(false);
	l->addWidget(m_widget);
	setCentralWidget(central);

	/* actions */
	TDEAction *reloadView = new TDEAction(i18n("Re&fresh View"), "reload", TDEStdAccel::shortcut(TDEStdAccel::Reload),
	                                      m_widget, TQ_SLOT(refresh()), actionCollection(), "refresh_view");
	TDEAction *reloadFw = new TDEAction(i18n("&Reload Firewall"), "reload", 0,
	                                    m_widget, TQ_SLOT(reloadFirewall()), actionCollection(), "reload_firewall");
	TDEAction *toPerm = new TDEAction(i18n("Runtime to &Permanent"), "document-save", 0,
	                                  m_widget, TQ_SLOT(runtimeToPermanent()), actionCollection(), "runtime_to_permanent");
	TDEAction *quit = KStdAction::quit(kapp, TQ_SLOT(quit()), actionCollection());

	TDEPopupMenu *file = new TDEPopupMenu(this);
	reloadView->plug(file);
	file->insertSeparator();
	reloadFw->plug(file);
	toPerm->plug(file);
	file->insertSeparator();
	quit->plug(file);
	menuBar()->insertItem(i18n("&File"), file);
	menuBar()->insertItem(i18n("&Help"), helpMenu());

	statusBar()->message(i18n("Ready"));
	connect(m_widget, TQ_SIGNAL(statusMessage(const TQString&)), TQ_SLOT(slotStatusMessage(const TQString&)));

	setAutoSaveSettings("MainWindow", true);
	resize(960, 680);
}

void MainWindow::slotStatusMessage(const TQString &message)
{
	statusBar()->message(message);
}

#include "mainwindow.moc"
