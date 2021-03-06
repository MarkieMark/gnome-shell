/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;

const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;
const Search = imports.ui.search;
const Main = imports.ui.main;

const THUMBNAIL_ICON_MARGIN = 2;

function DocInfo(recentInfo) {
    this._init(recentInfo);
}

DocInfo.prototype = {
    _init : function(recentInfo) {
        this.recentInfo = recentInfo;
        // We actually used get_modified() instead of get_visited()
        // here, as GtkRecentInfo doesn't updated get_visited()
        // correctly. See http://bugzilla.gnome.org/show_bug.cgi?id=567094
        this.timestamp = recentInfo.get_modified().getTime() / 1000;
        this.name = recentInfo.get_display_name();
        this._lowerName = this.name.toLowerCase();
        this.uri = recentInfo.get_uri();
        this.mimeType = recentInfo.get_mime_type();
    },

    createIcon : function(size) {
        return St.TextureCache.get_default().load_recent_thumbnail(size, this.recentInfo);
    },

    launch : function() {
        Shell.DocSystem.get_default().open(this.recentInfo);
    },

    matchTerms: function(terms) {
        let mtype = Search.MatchType.NONE;
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            let idx = this._lowerName.indexOf(term);
            if (idx == 0) {
                if (mtype != Search.MatchType.NONE)
                    return Search.MatchType.MULTIPLE;
                mtype = Search.MatchType.PREFIX;
            } else if (idx > 0) {
                if (mtype != Search.MatchType.NONE)
                    return Search.MatchType.MULTIPLE;
                mtype = Search.MatchType.SUBSTRING;
            } else {
                continue;
            }
        }
        return mtype;
    }
};

var docManagerInstance = null;

function getDocManager() {
    if (docManagerInstance == null)
        docManagerInstance = new DocManager();
    return docManagerInstance;
}

/**
 * DocManager wraps the DocSystem, primarily to expose DocInfo objects
 * which conform to the GenericDisplay item API.
 */
function DocManager() {
    this._init();
}

DocManager.prototype = {
    _init: function() {
        this._docSystem = Shell.DocSystem.get_default();
        this._infosByTimestamp = [];
        this._infosByUri = {};
        this._docSystem.connect('changed', Lang.bind(this, this._reload));
        this._reload();
    },

    _reload: function() {
        let docs = this._docSystem.get_all();
        this._infosByTimestamp = [];
        this._infosByUri = {};
        for (let i = 0; i < docs.length; i++) {
            let recentInfo = docs[i];

            let docInfo = new DocInfo(recentInfo);
            this._infosByTimestamp.push(docInfo);
            this._infosByUri[docInfo.uri] = docInfo;
        }
        this.emit('changed');
    },

    getTimestampOrderedInfos: function() {
        return this._infosByTimestamp;
    },

    getInfosByUri: function() {
        return this._infosByUri;
    },

    lookupByUri: function(uri) {
        return this._infosByUri[uri];
    },

    queueExistenceCheck: function(count) {
        return this._docSystem.queue_existence_check(count);
    },

    initialSearch: function(terms) {
        let multipleMatches = [];
        let prefixMatches = [];
        let substringMatches = [];
        for (let i = 0; i < this._infosByTimestamp.length; i++) {
            let item = this._infosByTimestamp[i];
            let mtype = item.matchTerms(terms);
            if (mtype == Search.MatchType.MULTIPLE)
                multipleMatches.push(item.uri);
            else if (mtype == Search.MatchType.PREFIX)
                prefixMatches.push(item.uri);
            else if (mtype == Search.MatchType.SUBSTRING)
                substringMatches.push(item.uri);
         }
        return multipleMatches.concat(prefixMatches.concat(substringMatches));
    },

    subsearch: function(previousResults, terms) {
        let multipleMatches = [];
        let prefixMatches = [];
        let substringMatches = [];
        for (let i = 0; i < previousResults.length; i++) {
            let uri = previousResults[i];
            let item = this._infosByUri[uri];
            let mtype = item.matchTerms(terms);
            if (mtype == Search.MatchType.MULTIPLE)
                multipleMatches.push(uri);
            else if (mtype == Search.MatchType.PREFIX)
                prefixMatches.push(uri);
            else if (mtype == Search.MatchType.SUBSTRING)
                substringMatches.push(uri);
        }
        return multipleMatches.concat(prefixMatches.concat(substringMatches));
    }
}

Signals.addSignalMethods(DocManager.prototype);
