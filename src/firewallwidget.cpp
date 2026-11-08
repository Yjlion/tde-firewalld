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

#include "firewallwidget.h"
#include "firewalldclient.h"
#include "statuspage.h"
#include "zonespage.h"
#include "servicespage.h"
#include "interfacespage.h"

#include <tqframe.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqtabwidget.h>
#include <tqwhatsthis.h>

#include <tdeapplication.h>
#include <kcombobox.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

static TQFrame *makeBanner(TQWidget *parent, const TQString &icon, TQLabel **text)
{
	TQFrame *frame = new TQFrame(parent);
	frame->setFrameStyle(TQFrame::StyledPanel | TQFrame::Raised);
	frame->setPaletteBackgroundColor(TQColor(255, 244, 206));
	frame->setPaletteForegroundColor(TQt::black);
	TQHBoxLayout *l = new TQHBoxLayout(frame, KDialog::marginHint() / 2, KDialog::spacingHint());
	TQLabel *pix = new TQLabel(frame);
	pix->setPixmap(DesktopIcon(icon, 32));
	l->addWidget(pix);
	*text = new TQLabel(frame);
	(*text)->setAlignment(TQt::WordBreak | TQt::AlignVCenter);
	l->addWidget(*text, 1);
	return frame;
}

FirewallWidget::FirewallWidget(TQWidget *parent, const char *name)
	: TQWidget(parent, name)
{
	m_client = new FirewalldClient(this);

	TQVBoxLayout *top = new TQVBoxLayout(this, 0, KDialog::spacingHint());

	/* banners */
	m_notRunning = makeBanner(this, "messagebox_warning", &m_notRunningText);
	KPushButton *start = new KPushButton(SmallIconSet("media-playback-start"), i18n("&Start firewalld"), m_notRunning);
	static_cast<TQBoxLayout*>(m_notRunning->layout())->addWidget(start);
	top->addWidget(m_notRunning);
	connect(start, TQ_SIGNAL(clicked()), TQ_SLOT(slotStartFirewalld()));

	TQLabel *agentText;
	m_noAgent = makeBanner(this, "encrypted", &agentText);
	agentText->setText(i18n("<b>No polkit authentication agent seems to be running.</b><br>"
	                        "Changes to the firewall require authentication. Please start "
	                        "<i>polkit-agent-tde</i> (it normally starts with your TDE session)."));
	top->addWidget(m_noAgent);

	TQLabel *authText;
	m_needAuth = makeBanner(this, "encrypted", &authText);
	authText->setText(i18n("<b>Authentication is required to view the firewall configuration.</b><br>"
	                       "Authentication was cancelled or failed."));
	KPushButton *auth = new KPushButton(SmallIconSet("unlock"), i18n("A&uthenticate..."), m_needAuth);
	static_cast<TQBoxLayout*>(m_needAuth->layout())->addWidget(auth);
	top->addWidget(m_needAuth);
	connect(auth, TQ_SIGNAL(clicked()), TQ_SLOT(slotAuthenticate()));

	/* mode selector and global actions */
	TQHBoxLayout *bar = new TQHBoxLayout(top);
	TQLabel *ml = new TQLabel(i18n("Co&nfiguration:"), this);
	m_mode = new KComboBox(false, this);
	m_mode->insertItem(SmallIcon("media-playback-start"), i18n("Runtime"));
	m_mode->insertItem(SmallIcon("document-save"), i18n("Permanent"));
	ml->setBuddy(m_mode);
	TQWhatsThis::add(m_mode, i18n("<p><b>Runtime</b> changes take effect immediately but are lost when "
	                               "firewalld is reloaded or the computer is restarted.</p>"
	                               "<p><b>Permanent</b> changes are saved to disk and take effect "
	                               "after the firewall is reloaded.</p>"));
	m_reload = new KPushButton(SmallIconSet("reload"), i18n("Reload Fire&wall"), this);
	TQWhatsThis::add(m_reload, i18n("Reload the permanent configuration. Runtime-only changes are lost."));
	m_toPermanent = new KPushButton(SmallIconSet("document-save"), i18n("Runtime to P&ermanent"), this);
	TQWhatsThis::add(m_toPermanent, i18n("Save the current runtime configuration as the permanent configuration."));
	bar->addWidget(ml);
	bar->addWidget(m_mode);
	bar->addStretch(1);
	bar->addWidget(m_reload);
	bar->addWidget(m_toPermanent);

	/* pages */
	m_tabs = new TQTabWidget(this);
	m_status = new StatusPage(m_client, m_tabs);
	m_zones = new ZonesPage(m_client, m_tabs);
	m_services = new ServicesPage(m_client, m_tabs);
	m_interfaces = new InterfacesPage(m_client, m_tabs);
	m_tabs->addTab(m_status, SmallIconSet("info"), i18n("S&tatus"));
	m_tabs->addTab(m_zones, SmallIconSet("network_local"), i18n("&Zones"));
	m_tabs->addTab(m_services, SmallIconSet("services"), i18n("Ser&vices"));
	m_tabs->addTab(m_interfaces, SmallIconSet("network"), i18n("&Interfaces"));
	top->addWidget(m_tabs, 1);

	m_message = new TQLabel(this);
	top->addWidget(m_message);

	connect(m_mode, TQ_SIGNAL(activated(int)), TQ_SLOT(slotModeChanged(int)));
	connect(m_reload, TQ_SIGNAL(clicked()), TQ_SLOT(reloadFirewall()));
	connect(m_toPermanent, TQ_SIGNAL(clicked()), TQ_SLOT(runtimeToPermanent()));
	connect(m_tabs, TQ_SIGNAL(currentChanged(TQWidget*)), TQ_SLOT(slotTabChanged(TQWidget*)));
	connect(m_client, TQ_SIGNAL(operationFinished(bool, const TQString&, const TQString&)),
	        TQ_SLOT(slotOperationFinished(bool, const TQString&, const TQString&)));
	connect(m_client, TQ_SIGNAL(configurationChanged()), TQ_SLOT(slotConfigurationChanged()));
	connect(m_client, TQ_SIGNAL(availabilityChanged(bool)), TQ_SLOT(slotAvailabilityChanged(bool)));

	updateBanners();
}

FirewallWidget::~FirewallWidget()
{
}

void FirewallWidget::showPage(const TQString &page)
{
	if (page == "status")
		m_tabs->showPage(m_status);
	else if (page == "zones")
		m_tabs->showPage(m_zones);
	else if (page == "services")
		m_tabs->showPage(m_services);
	else if (page == "interfaces")
		m_tabs->showPage(m_interfaces);
}

void FirewallWidget::setMessageLineVisible(bool visible)
{
	if (visible)
		m_message->show();
	else
		m_message->hide();
}

void FirewallWidget::selectZone(const TQString &zone)
{
	m_zones->selectZone(zone);
	m_dirty[m_zones] = true;
}

void FirewallWidget::setPermanent(bool permanent)
{
	m_mode->setCurrentItem(permanent ? 1 : 0);
	slotModeChanged(permanent ? 1 : 0);
}

void FirewallWidget::updateBanners()
{
	bool running = m_client->isRunning();
	if (!m_client->busConnected())
		m_notRunningText->setText(i18n("<b>Cannot connect to the D-Bus system bus.</b>"));
	else
		m_notRunningText->setText(i18n("<b>The firewalld service is not running.</b><br>"
		                               "The firewall configuration cannot be shown or changed "
		                               "until firewalld is started."));
	if (running)
		m_notRunning->hide();
	else
		m_notRunning->show();

	if (FirewalldClient::polkitAgentRunning())
		m_noAgent->hide();
	else
		m_noAgent->show();

	if (running && m_client->authorizationDenied())
		m_needAuth->show();
	else
		m_needAuth->hide();

	m_mode->setEnabled(running);
	m_reload->setEnabled(running);
	m_toPermanent->setEnabled(running);
}

void FirewallWidget::refreshPage(TQWidget *page)
{
	TQApplication::setOverrideCursor(TQt::waitCursor);
	if (page == m_status)
		m_status->refresh();
	else if (page == m_zones)
		m_zones->refresh();
	else if (page == m_services)
		m_services->refresh();
	else if (page == m_interfaces)
		m_interfaces->refresh();
	m_dirty[page] = false;
	TQApplication::restoreOverrideCursor();
	updateBanners();
}

void FirewallWidget::refresh()
{
	updateBanners();
	m_dirty[m_status] = m_dirty[m_zones] = m_dirty[m_services] = m_dirty[m_interfaces] = true;
	refreshPage(m_tabs->currentPage());
}

void FirewallWidget::slotTabChanged(TQWidget *page)
{
	if (!m_dirty.contains(page) || m_dirty[page])
		refreshPage(page);
}

void FirewallWidget::slotModeChanged(int index)
{
	m_client->setMode(index == 1 ? FirewalldClient::Permanent : FirewalldClient::Runtime);
	m_zones->clearCache();
	refresh();
}

void FirewallWidget::slotConfigurationChanged()
{
	refresh();
}

void FirewallWidget::slotAvailabilityChanged(bool)
{
	m_zones->clearCache();
	refresh();
}

void FirewallWidget::slotOperationFinished(bool success, const TQString &description, const TQString &error)
{
	if (success) {
		TQString msg = i18n("Done: %1").arg(description);
		if (m_client->mode() == FirewalldClient::Permanent)
			msg += " " + i18n("(reload the firewall to apply permanent changes)");
		m_message->setText(msg);
		emit statusMessage(msg);
	} else {
		m_message->setText(i18n("Failed: %1").arg(description));
		emit statusMessage(i18n("Failed: %1").arg(description));
		KMessageBox::sorry(this, TQString("<p><b>%1</b></p><p>%2</p>").arg(description).arg(error),
		                   i18n("Firewall Change Failed"));
		/* reset widgets to the real state */
		refresh();
	}
}

void FirewallWidget::reloadFirewall()
{
	m_client->reload();
}

void FirewallWidget::runtimeToPermanent()
{
	int res = KMessageBox::questionYesNo(this,
		i18n("Replace the permanent configuration with the current runtime configuration?"),
		i18n("Runtime to Permanent"), KGuiItem(i18n("Make Permanent"), "document-save"), KStdGuiItem::cancel());
	if (res == KMessageBox::Yes)
		m_client->runtimeToPermanent();
}

void FirewallWidget::slotAuthenticate()
{
	m_client->resetAuthorization();
	refresh();
}

void FirewallWidget::slotStartFirewalld()
{
	TQApplication::setOverrideCursor(TQt::waitCursor);
	bool ok = m_client->startFirewalld();
	TQApplication::restoreOverrideCursor();
	if (!ok)
		KMessageBox::sorry(this, i18n("<p>firewalld could not be started.</p><p>%1</p>")
		                   .arg(m_client->lastError()));
	refresh();
}

#include "firewallwidget.moc"
