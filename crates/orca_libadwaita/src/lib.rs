// lib.rs
//
// Copyright 2022 William Roy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod application;
mod config;
mod window;

use gettextrs::bind_textdomain_codeset;
use gettextrs::bindtextdomain;
use gettextrs::textdomain;

use gtk::gio;
use gtk::gio::resources_register_include;
use gtk::prelude::ApplicationExtManual;

use application::OrcaApplication;
use config::GETTEXT_PACKAGE;
use config::LOCALEDIR;
use window::OrcaWindow;

pub fn main() {
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR).expect("Unable to bind the text domain");
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8").expect("Unable to set the text domain encoding");
  textdomain(GETTEXT_PACKAGE).expect("Unable to switch to the text domain");

  resources_register_include!("orca.gresource").expect("Could not load resources");
  let app = OrcaApplication::new("com.github.wroyca.orca", &gio::ApplicationFlags::empty());
  std::process::exit(app.run());
}
