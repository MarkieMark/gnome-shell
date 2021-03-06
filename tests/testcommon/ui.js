/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Environment = imports.ui.environment;

function init() {
    Clutter.init(null, null);
    Environment.init();

    let stage = Clutter.Stage.get_default();
    let context = St.ThemeContext.get_for_stage (stage);
    let stylesheetPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/test.css";
    let theme = new St.Theme ({ application_stylesheet: stylesheetPath });
    context.set_theme (theme);
}
