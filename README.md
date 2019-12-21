# Sequoia

A browser-like application for tab hoarders, based on Microsoft Edge WebView2.

This is very much a work in progress, and there is no documentation.

### Requirements

To build Sequoia, you need Visual Studio 2019.  2017 will probably work if you reconfigure the project.

To run Sequoia, you must have Microsoft Edge (Chromium-based) installed.

### Usage

There are very few features so far, but currently you can open and close tabs, which are shown in a tree on the side of the application window.  When you middle-click a link, it will open a tab as a child of the current tab, but will not load it until you click on it.  All tabs are saved to a database file, so you can freely quit and restart the application without losing your tabs.
