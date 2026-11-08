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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mainwindow.h"
#include "firewallwidget.h"

#include <tdeaboutdata.h>
#include <tdeapplication.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>

static const char description[] = I18N_NOOP("Firewall configuration (firewalld) for the Trinity Desktop Environment");

static TDECmdLineOptions options[] = {
	{ "permanent", I18N_NOOP("Start in permanent configuration mode"), 0 },
	{ "page <page>", I18N_NOOP("Initial page: status, zones, services or interfaces"), "status" },
	{ "zone <zone>", I18N_NOOP("Zone to show on the zones page"), 0 },
	TDECmdLineLastOption
};

int main(int argc, char **argv)
{
	TDEAboutData about("tde-firewalld", I18N_NOOP("Firewall Manager"), TDE_FIREWALLD_VERSION, description,
	                   TDEAboutData::License_GPL, "(C) 2026 tde-firewalld developers", 0,
	                   "https://github.com/Yjlion/tde-firewalld");
	about.addAuthor("tde-firewalld developers", 0, 0, "https://github.com/Yjlion/tde-firewalld");

	TDECmdLineArgs::init(argc, argv, &about);
	TDECmdLineArgs::addCmdLineOptions(options);
	TDEApplication app;

	TDECmdLineArgs *args = TDECmdLineArgs::parsedArgs();

	MainWindow *win = new MainWindow();
	app.setMainWidget(win);
	if (args->isSet("zone"))
		win->firewallWidget()->selectZone(TQString::fromLocal8Bit(args->getOption("zone")));
	if (args->isSet("permanent"))
		win->firewallWidget()->setPermanent(true);
	else
		win->firewallWidget()->refresh();
	win->firewallWidget()->showPage(TQString::fromLocal8Bit(args->getOption("page")));
	args->clear();

	win->show();
	return app.exec();
}
