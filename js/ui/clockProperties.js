const St = imports.gi.St;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

function ClockProperties(panel, isSecVisible, isDateVisible) {
    this._init(panel, isSecVisible, isDateVisible);
}

ClockProperties.prototype = {
    _init: function(panel, isSecVisible, isDateVisible) {
	this.actor = new St.Table({homogeneous: false,
				  style_class: "properties",
				  reactive: true});
	this._isSecVisible = isSecVisible;
	this._isDateVisible = isDateVisible;
	var key;
	for (key in Shell.GConf)
		print(String(key) + ":" + String(Shell.GConf[key]));
	this._gconf = Shell.GConf.get_default();
	for (key in this._gconf)
		print(String(key) + ":" + String(this._gconf[key]));
	this._panel = panel;
	this._okLabel = /*"☑"*/ "<b>&#9745;</b>";
	this._noLabel = /*"☐"*/ "<b>&#9744;</b>";
	let dateLabel = new St.Label({text: _("View Date")});
	this.actor.add(dateLabel, {row: 0, col: 0});
	this._dateCheckInd = this.actor.get_children().length;
	this._update();
    },

    _update: function() {
	let dateCheck = new St.Button({label: 
	      (this._isDateVisible ? this._okLabel : this._noLabel), 
	      style_class: "properties-checkbox"});
	let secCheck = new St.Button({label: 
	      (this._isSecVisible ? this._okLabel : this._noLabel),
	      style_class: "properties-checkbox"});
	let secLabel = new St.Label({text: _("View Seconds")});
	this.actor.add(dateCheck, {row: 0, col: 1});
	dateCheck.connect('clicked', Lang.bind(this, this._toggleDateVisibility));
	this.actor.add(secLabel, {row: 1, col:0});
	this.actor.add(secCheck, {row: 1, col:1});
	secCheck.connect('clicked', Lang.bind(this, this._toggleSecVisibility));
    },

    _toggleDateVisibility: function() {
	this._isDateVisible = !(this._isDateVisible);
	let c = this.actor.get_children();
	for (let i = this._dateCheckInd; i < c.length; i++)
	    c[i].destroy();
	this._update();
	this._panel.setClockDateVisible(this._isDateVisible);
	this._gconf.set_boolean('/apps/gnome-shell/panel_clock/prefs/show_date',
							this._isDateVisible);
    },

    _toggleSecVisibility: function() {
	this._isSecVisible = !(this._isSecVisible);
	let c = this.actor.get_children();
	for (let i = this._dateCheckInd; i < c.length; i++)
	    c[i].destroy();
	this._update();
	this._panel.setClockSecVisible(this._isSecVisible);
	this._gconf.set_boolean('/apps/gnome-shell/panel_clock/prefs/show_seconds',
							this._isSecVisible);
    }
}
