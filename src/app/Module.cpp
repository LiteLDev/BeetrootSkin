#include "app/Module.h"

#include "mod/Log.h"

#include <algorithm>
#include <exception>

namespace beetroot::app {

namespace {
std::vector<Module*> gInstalled;
}

std::vector<Module*>& modules() {
    static std::vector<Module*> instance;
    return instance;
}

namespace {
std::vector<Module*> sortedModules() {
    auto sorted = modules();
    std::stable_sort(sorted.begin(), sorted.end(), [](Module* a, Module* b) { return a->order() < b->order(); });
    return sorted;
}
} // namespace

void loadModules() {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    for (auto* m : sortedModules()) {
        try {
            m->onLoad();
        } catch (std::exception const& e) {
            log().error("Module {} failed to load: {}", m->name(), e.what());
        } catch (...) {
            log().error("Module {} failed to load", m->name());
        }
    }
}

void installModules() {
    for (auto* m : sortedModules()) {
        try {
            m->install();
            gInstalled.push_back(m);
        } catch (std::exception const& e) {
            log().error("Module {} failed to install: {}", m->name(), e.what());
        } catch (...) {
            log().error("Module {} failed to install", m->name());
        }
    }
    log().debug("{} modules installed", gInstalled.size());
}

void uninstallModules() {
    for (auto it = gInstalled.rbegin(); it != gInstalled.rend(); ++it) {
        try {
            (*it)->uninstall();
        } catch (std::exception const& e) {
            log().error("Module {} failed to uninstall: {}", (*it)->name(), e.what());
        } catch (...) {
            log().error("Module {} failed to uninstall", (*it)->name());
        }
    }
    gInstalled.clear();
}

} // namespace beetroot::app
