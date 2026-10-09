// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "page_settings.h"
#include "widgets.h"

#include <string>

namespace tuxblox {

namespace {

const char *const Channels[] = {"stable", "canary", "experimental", nullptr};

// What settings.json stores, in the order the dropdown offers them
const char *const Themes[] = {"system", "dark", "grey", "light", nullptr};
const char *const ThemeLabels[] = {"System theme", "Dark", "Grey", "Light", nullptr};

std::string automaticGpuLabel(const std::vector<GpuDevice> &gpus) {
    if (gpus.empty()) return "Automatic";
    return "Automatic (1: " + gpus.front().label + ")";
}

} // namespace

SettingsPage::SettingsPage(App &app) : app_(app) {
    gpus_ = enumerateGpus("/sys/class/drm");
    pRoot_ = adw_preferences_page_new();

    AdwPreferencesGroup *pLauncher = addGroup("Launcher");
    buildTheme(pLauncher);
    addToggle(pLauncher, "Minimize to background", "Automatically close this window whenever Roblox starts.",
              &Settings::minimizeToBackground);

    buildAccount();

    buildUpdates();
    buildEnvironment();

    AdwPreferencesGroup *pController = addGroup("Controller");
    addToggle(pController, "Enable Haptics", "An experimental feature that lets Roblox vibrate your controller",
              &Settings::haptics);

    AdwPreferencesGroup *pTroubleshooting = addGroup("Troubleshooting");
    addToggle(pTroubleshooting, "Detailed logging",
              "Records much more information in session logs. Turn this on if you're reporting a bug.",
              &Settings::debugLogging);

    AdwPreferencesGroup *pPrivacy = addGroup("Privacy");
    addToggle(pPrivacy, "Always send crash reports",
              "Send the session log and your versions when Roblox crashes. See tuxblox.net/privacy",
              &Settings::sendCrashReports);
    addToggle(pPrivacy, "Show what you are doing on Discord",
              "Show the place you have open in Studio on your Discord profile.", &Settings::discordRpc);

    buildDangerZone();
}

GtkWidget *SettingsPage::widget() const {
    return pRoot_;
}

AdwPreferencesGroup *SettingsPage::addGroup(const char *pTitle) {
    GtkWidget *pGroup = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(pGroup), pTitle);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pGroup));
    return ADW_PREFERENCES_GROUP(pGroup);
}

void SettingsPage::addToggle(AdwPreferencesGroup *pGroup, const char *pTitle, const char *pSubtitle,
                             bool Settings::*pField) {
    GtkWidget *pRow = plainSwitchRow(pTitle, pSubtitle);
    toggles_.push_back(std::make_unique<Toggle>(Toggle{this, pField, ADW_SWITCH_ROW(pRow)}));
    g_signal_connect(pRow, "notify::active", G_CALLBACK(onToggle), toggles_.back().get());
    adw_preferences_group_add(pGroup, pRow);
}

void SettingsPage::buildTheme(AdwPreferencesGroup *pGroup) {
    GtkWidget *pTheme = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pTheme), "Launcher theme");
    GtkStringList *pThemes = gtk_string_list_new(ThemeLabels);
    adw_combo_row_set_model(ADW_COMBO_ROW(pTheme), G_LIST_MODEL(pThemes));
    g_object_unref(pThemes);
    pTheme_ = ADW_COMBO_ROW(pTheme);
    g_signal_connect(pTheme, "notify::selected", G_CALLBACK(onTheme), this);
    adw_preferences_group_add(pGroup, pTheme);
}

void SettingsPage::buildAccount() {
    AdwPreferencesGroup *pGroup = addGroup("Roblox account");
    adw_preferences_group_set_description(
        pGroup, "Saved on this computer, in a file only your user can read. A verification code or a captcha "
                "still has to be finished by hand.");

    GtkWidget *pSwitch = plainSwitchRow("Sign in automatically",
                                        "Log in to Studio with this account. Turning this off forgets it.");
    pAutoSignIn_ = ADW_SWITCH_ROW(pSwitch);
    g_signal_connect(pSwitch, "notify::active", G_CALLBACK(onAutoSignIn), this);
    adw_preferences_group_add(pGroup, pSwitch);

    GtkWidget *pUsername = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pUsername), "Username, email or phone");
    pUsername_ = ADW_ENTRY_ROW(pUsername);
    gtk_widget_set_sensitive(pUsername, FALSE);
    g_signal_connect(pUsername, "changed", G_CALLBACK(onSignInChanged), this);
    adw_preferences_group_add(pGroup, pUsername);

    GtkWidget *pPassword = adw_password_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pPassword), "Password");
    pPassword_ = ADW_PASSWORD_ENTRY_ROW(pPassword);
    gtk_widget_set_sensitive(pPassword, FALSE);
    g_signal_connect(pPassword, "changed", G_CALLBACK(onSignInChanged), this);
    adw_preferences_group_add(pGroup, pPassword);

    // Not a row, so the group places it below its list
    pSignInError_ = errorLabel();
    adw_preferences_group_add(pGroup, pSignInError_);
}

void SettingsPage::buildUpdates() {
    AdwPreferencesGroup *pGroup = addGroup("Updates");

    GtkWidget *pChannel = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pChannel), "Update channel");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(pChannel), "The release channel TuxBlox updates to");
    GtkStringList *pChannels = gtk_string_list_new(Channels);
    adw_combo_row_set_model(ADW_COMBO_ROW(pChannel), G_LIST_MODEL(pChannels));
    g_object_unref(pChannels);
    pChannel_ = ADW_COMBO_ROW(pChannel);
    g_signal_connect(pChannel, "notify::selected", G_CALLBACK(onChannel), this);
    adw_preferences_group_add(pGroup, pChannel);

    addToggle(pGroup, "Automatic updates", "Install TuxBlox updates without asking.", &Settings::autoUpdate);
    addToggle(pGroup, "Auto-Update Roblox", "", &Settings::autoUpdateRoblox);
}

void SettingsPage::buildEnvironment() {
    AdwPreferencesGroup *pGroup = addGroup("Environment");

    GtkWidget *pGpu = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pGpu), "Graphics card");
    GtkStringList *pGpus = gtk_string_list_new(nullptr);
    gtk_string_list_append(pGpus, automaticGpuLabel(gpus_).c_str());
    for (size_t i = 0; i < gpus_.size(); ++i) {
        gtk_string_list_append(pGpus, (std::to_string(i + 1) + ": " + gpus_[i].label).c_str());
    }
    adw_combo_row_set_model(ADW_COMBO_ROW(pGpu), G_LIST_MODEL(pGpus));
    g_object_unref(pGpus);
    pGpu_ = ADW_COMBO_ROW(pGpu);
    g_signal_connect(pGpu, "notify::selected", G_CALLBACK(onGpu), this);
    adw_preferences_group_add(pGroup, pGpu);

    addToggle(pGroup, "GPU acceleration for web pages",
              "Speed up the rendering of web panels using your graphics card", &Settings::webviewGpu);
    addToggle(pGroup, "Virtual Desktop Mode", "Run Roblox in a separate window that acts as a desktop",
              &Settings::virtualDesktop);

    // An entry beside the row rather than an AdwEntryRow, which puts the entry on its own line
    GtkWidget *pRow = plainActionRow("Environment variables", "");
    GtkWidget *pEntry = gtk_entry_new();
    pEnvironment_ = GTK_ENTRY(pEntry);
    gtk_entry_set_placeholder_text(pEnvironment_, "VARIABLE=value");
    gtk_editable_set_width_chars(GTK_EDITABLE(pEntry), 24);
    gtk_widget_set_valign(pEntry, GTK_ALIGN_CENTER);
    g_signal_connect(pEntry, "activate", G_CALLBACK(onEnvironmentActivate), this);
    GtkEventController *pFocus = gtk_event_controller_focus_new();
    g_signal_connect(pFocus, "leave", G_CALLBACK(onEnvironmentFocusLeft), this);
    gtk_widget_add_controller(pEntry, pFocus);
    adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pEntry);
    adw_preferences_group_add(pGroup, pRow);
}

GtkButton *SettingsPage::dangerButton(GtkWidget *pRow, const char *pLabel, GCallback onClicked) {
    GtkWidget *pButton = gtk_button_new_with_label(pLabel);
    gtk_widget_add_css_class(pButton, "destructive-action");
    gtk_widget_set_valign(pButton, GTK_ALIGN_CENTER);
    g_signal_connect(pButton, "clicked", onClicked, this);
    adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pButton);
    return GTK_BUTTON(pButton);
}

void SettingsPage::buildDangerZone() {
    AdwPreferencesGroup *pGroup = addGroup("Danger zone");

    GtkWidget *pTerminate = plainActionRow("Terminate Roblox", "");
    pTerminate_ = dangerButton(pTerminate, "Terminate", G_CALLBACK(onTerminate));
    adw_preferences_group_add(pGroup, pTerminate);

    GtkWidget *pWipe = plainActionRow("Wipe the virtual drive", "");
    pWipe_ = dangerButton(pWipe, "Wipe prefix", G_CALLBACK(onWipe));
    adw_preferences_group_add(pGroup, pWipe);

    GtkWidget *pUninstall = plainActionRow("Uninstall TuxBlox", "");
    pUninstall_ = dangerButton(pUninstall, "Uninstall", G_CALLBACK(onUninstall));
    adw_preferences_group_add(pGroup, pUninstall);

    // Not rows, so the group places them below its list
    pWipeError_ = errorLabel();
    adw_preferences_group_add(pGroup, pWipeError_);
    pUninstallError_ = errorLabel();
    adw_preferences_group_add(pGroup, pUninstallError_);
}

void SettingsPage::onToggle(GObject *pRow, GParamSpec *, gpointer data) {
    auto *pToggle = static_cast<Toggle *>(data);
    SettingsPage &self = *pToggle->pOwner;
    if (self.seeding_) return;
    Settings updated = self.app_.snapshot().settings;
    updated.*(pToggle->pField) = adw_switch_row_get_active(ADW_SWITCH_ROW(pRow));
    self.app_.updateSettings(updated);
}

void SettingsPage::onChannel(GObject *pRow, GParamSpec *, gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    if (pSelf->seeding_) return;
    const guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(pRow));
    if (selected >= 3) return;
    Settings updated = pSelf->app_.snapshot().settings;
    updated.channel = Channels[selected];
    pSelf->app_.updateSettings(updated);
}

void SettingsPage::onTheme(GObject *pRow, GParamSpec *, gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    if (pSelf->seeding_) return;
    const guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(pRow));
    if (selected >= G_N_ELEMENTS(Themes) - 1) return;
    Settings updated = pSelf->app_.snapshot().settings;
    updated.theme = Themes[selected];
    pSelf->app_.updateSettings(updated);
    // Straight away rather than on the next poll, so the window changes colour as the row is clicked
    applyLauncherTheme(updated.theme);
}

void SettingsPage::onGpu(GObject *pRow, GParamSpec *, gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    if (pSelf->seeding_) return;
    const guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(pRow));
    if (selected == GTK_INVALID_LIST_POSITION || selected > pSelf->gpus_.size()) return;
    Settings updated = pSelf->app_.snapshot().settings;
    updated.gpu = selected == 0 ? "" : pSelf->gpus_[selected - 1].pciAddress;
    pSelf->app_.updateSettings(updated);
}

void SettingsPage::commitEnvironmentVariables() {
    if (seeding_) return;
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(pEnvironment_));
    Settings updated = app_.snapshot().settings;
    if (updated.envVars == text) return;
    updated.envVars = text;
    app_.updateSettings(updated);
}

// Saved as it is typed rather than on Enter, so leaving the page mid-edit cannot lose it. The file is replaced in one rename each time, so the layer never reads a half-written one.
void SettingsPage::commitSignIn() {
    if (seeding_ || !adw_switch_row_get_active(pAutoSignIn_)) return;
    const SignInCredentials credentials{gtk_editable_get_text(GTK_EDITABLE(pUsername_)),
                                        gtk_editable_get_text(GTK_EDITABLE(pPassword_))};
    showError(pSignInError_, app_.saveSignIn(credentials) ? ""
                                                          : "The account could not be saved. Check that the TuxBlox "
                                                            "folder can be written to.");
}

void SettingsPage::setSignInFields(const SignInCredentials &credentials) {
    const bool wasSeeding = seeding_;
    seeding_ = true;
    gtk_editable_set_text(GTK_EDITABLE(pUsername_), credentials.username.c_str());
    gtk_editable_set_text(GTK_EDITABLE(pPassword_), credentials.password.c_str());
    seeding_ = wasSeeding;
}

void SettingsPage::onAutoSignIn(GObject *pRow, GParamSpec *, gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    const bool active = adw_switch_row_get_active(ADW_SWITCH_ROW(pRow));
    gtk_widget_set_sensitive(GTK_WIDGET(pSelf->pUsername_), active);
    gtk_widget_set_sensitive(GTK_WIDGET(pSelf->pPassword_), active);
    if (pSelf->seeding_) return;

    Settings updated = pSelf->app_.snapshot().settings;
    updated.autoSignIn = active;
    pSelf->app_.updateSettings(updated);
    if (active) {
        pSelf->commitSignIn();
        return;
    }
    // Off forgets the account rather than keeping it for later: a password has no business staying on disk for a feature that is not being used.
    pSelf->setSignInFields(SignInCredentials{});
    showError(pSelf->pSignInError_, pSelf->app_.forgetSignIn()
                                        ? ""
                                        : "The saved account could not be deleted. Remove the sign-in file in the "
                                          "TuxBlox folder by hand.");
}

void SettingsPage::onSignInChanged(GtkEditable *, gpointer data) {
    static_cast<SettingsPage *>(data)->commitSignIn();
}

void SettingsPage::onEnvironmentActivate(GtkEntry *, gpointer data) {
    static_cast<SettingsPage *>(data)->commitEnvironmentVariables();
}

void SettingsPage::onEnvironmentFocusLeft(GtkEventControllerFocus *, gpointer data) {
    static_cast<SettingsPage *>(data)->commitEnvironmentVariables();
}

void SettingsPage::onTerminate(GtkButton *pButton, gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    pSelf->terminateAwaited_ = true;
    pSelf->app_.requestTerminateProcesses();
    setButtonLabel(pButton, "Stopping\xE2\x80\xA6");
    gtk_widget_set_sensitive(GTK_WIDGET(pButton), FALSE);
}

gboolean SettingsPage::onTerminateReset(gpointer data) {
    auto *pSelf = static_cast<SettingsPage *>(data);
    setButtonLabel(pSelf->pTerminate_, "Terminate");
    gtk_widget_set_sensitive(GTK_WIDGET(pSelf->pTerminate_), TRUE);
    return G_SOURCE_REMOVE;
}

void SettingsPage::onWipe(GtkButton *pButton, gpointer data) {
    App *pApp = &static_cast<SettingsPage *>(data)->app_;
    confirmDestructive(GTK_WIDGET(pButton), "Wipe the virtual drive?",
                       "Every installed Roblox goes with it. Your settings and FastFlags are kept.", "Wipe",
                       [pApp] { pApp->requestWipePrefix(); });
}

void SettingsPage::onUninstall(GtkButton *pButton, gpointer data) {
    App *pApp = &static_cast<SettingsPage *>(data)->app_;
    confirmDestructive(GTK_WIDGET(pButton), "Uninstall TuxBlox?",
                       "Removes the virtual drive, the shortcuts and TuxBlox itself.", "Uninstall",
                       [pApp] { pApp->requestUninstall(); });
}

void SettingsPage::seed(const Settings &settings) {
    seeding_ = true;
    for (guint i = 0; Channels[i] != nullptr; ++i) {
        if (settings.channel == Channels[i]) adw_combo_row_set_selected(pChannel_, i);
    }
    for (guint i = 0; Themes[i] != nullptr; ++i) {
        if (settings.theme == Themes[i]) adw_combo_row_set_selected(pTheme_, i);
    }
    guint gpuIndex = 0;
    for (size_t i = 0; i < gpus_.size(); ++i) {
        if (!settings.gpu.empty() && gpus_[i].pciAddress == settings.gpu) gpuIndex = static_cast<guint>(i + 1);
    }
    adw_combo_row_set_selected(pGpu_, gpuIndex);
    gtk_editable_set_text(GTK_EDITABLE(pEnvironment_), settings.envVars.c_str());
    for (const auto &toggle : toggles_) adw_switch_row_set_active(toggle->pRow, settings.*(toggle->pField));
    // notify::active only fires on a change, so with the switch left at its default the fields' sensitivity would never be set
    adw_switch_row_set_active(pAutoSignIn_, settings.autoSignIn);
    gtk_widget_set_sensitive(GTK_WIDGET(pUsername_), settings.autoSignIn);
    gtk_widget_set_sensitive(GTK_WIDGET(pPassword_), settings.autoSignIn);
    setSignInFields(settings.autoSignIn ? app_.savedSignIn() : SignInCredentials{});
    seeding_ = false;
    seeded_ = true;
}

void SettingsPage::update(const AppSnapshot &snap) {
    // Seeded once: after that the widgets are the source of truth, and re-seeding would overwrite what the user is typing
    if (!seeded_) seed(snap.settings);

    gtk_widget_set_sensitive(GTK_WIDGET(pWipe_), !snap.wipePrefix.inProgress);
    setButtonLabel(pWipe_, snap.wipePrefix.inProgress ? "Wiping\xE2\x80\xA6" : "Wipe prefix");
    showError(pWipeError_, snap.wipePrefix.errorMessage);

    gtk_widget_set_sensitive(GTK_WIDGET(pUninstall_), !snap.uninstall.inProgress);
    setButtonLabel(pUninstall_, snap.uninstall.inProgress ? "Uninstalling\xE2\x80\xA6" : "Uninstall");
    showError(pUninstallError_, snap.uninstall.errorMessage);

    // Closing the sessions properly takes seconds, so this follows the snapshot the way Wipe does rather than a return value, which used to freeze the window while it worked.
    if (terminateAwaited_ && !snap.terminate.inProgress && snap.terminate.signalled >= 0) {
        terminateAwaited_ = false;
        setButtonLabel(pTerminate_, snap.terminate.signalled == 0
                                        ? "Nothing running"
                                        : "Stopped " + std::to_string(snap.terminate.signalled));
        g_timeout_add(2500, onTerminateReset, this);
    }
}

} // namespace tuxblox
