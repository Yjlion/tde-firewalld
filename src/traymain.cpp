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

#include "firewalltray.h"

#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>
#include <kuniqueapplication.h>

static const char description[] = I18N_NOOP("System tray icon for the firewalld firewall");

int main(int argc, char **argv)
{
	TDEAboutData about("tde-firewalld-tray", I18N_NOOP("Firewall Tray"), TDE_FIREWALLD_VERSION, description,
	                   TDEAboutData::License_GPL, "(C) 2026 tde-firewalld developers", 0,
	                   "https://github.com/Yjlion/tde-firewalld");
	about.addAuthor("tde-firewalld developers", 0, 0, "https://github.com/Yjlion/tde-firewalld");

	TDECmdLineArgs::init(argc, argv, &about);
	KUniqueApplication::addCmdLineOptions();

	if (!KUniqueApplication::start())
		return 0;

	KUniqueApplication app;
	app.disableSessionManagement();

	FirewallTray *tray = new FirewallTray();
	app.setMainWidget(tray);
	tray->show();

	return app.exec();
}
