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

#include "kcm_firewalld.h"
#include "firewallwidget.h"

#include <tqlayout.h>

#include <tdeaboutdata.h>
#include <kgenericfactory.h>
#include <tdelocale.h>

typedef KGenericFactory<KCMFirewalld, TQWidget> KCMFirewalldFactory;
K_EXPORT_COMPONENT_FACTORY(kcm_firewalld, KCMFirewalldFactory("tde-firewalld"))

KCMFirewalld::KCMFirewalld(TQWidget *parent, const char *name, const TQStringList &)
	: TDECModule(parent, name)
{
	TDEAboutData *about = new TDEAboutData("kcmfirewalld", I18N_NOOP("Firewall"), TDE_FIREWALLD_VERSION,
		I18N_NOOP("Firewall configuration (firewalld) for the Trinity Desktop Environment"),
		TDEAboutData::License_GPL, "(C) 2026 tde-firewalld developers");
	about->addAuthor("tde-firewalld developers", 0, 0, "https://github.com/Yjlion/tde-firewalld");
	setAboutData(about);

	/* changes are applied immediately through firewalld */
	setButtons(Help);
	setUseRootOnlyMsg(false);

	TQVBoxLayout *layout = new TQVBoxLayout(this, 0, 0);
	m_widget = new FirewallWidget(this);
	m_widget->setMinimumSize(760, 540);
	layout->addWidget(m_widget);

	load();
}

KCMFirewalld::~KCMFirewalld()
{
}

void KCMFirewalld::load()
{
	m_widget->refresh();
}

TQString KCMFirewalld::quickHelp() const
{
	return i18n("<h1>Firewall</h1>"
	            "<p>This module configures the <b>firewalld</b> firewall. Network "
	            "connections and interfaces are assigned to <i>zones</i>; each zone "
	            "decides which services and ports are reachable from the network.</p>"
	            "<p>Changes are applied immediately. <b>Runtime</b> changes are lost on "
	            "reload or reboot, <b>Permanent</b> changes are saved and applied after "
	            "a reload.</p>"
	            "<p>Changing the firewall requires administrator authentication, which "
	            "is requested through polkit by <i>polkit-agent-tde</i>.</p>");
}

#include "kcm_firewalld.moc"
