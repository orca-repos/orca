// application.rs
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

use glib::clone;

use gtk::gio;
use gtk::glib;
use gtk::prelude::*;
use gtk::subclass::prelude::*;

use libadwaita::subclass::prelude::*;

use crate::config::VERSION;
use crate::OrcaWindow;

mod imp {
  use super::*;

  #[derive(Debug, Default)]
  pub struct OrcaApplication {}

  #[glib::object_subclass]
  impl ObjectSubclass for OrcaApplication {
    type ParentType = libadwaita::Application;
    type Type = super::OrcaApplication;

    const NAME: &'static str = "OrcaApplication";
  }

  impl ObjectImpl for OrcaApplication {
    fn constructed(&self, obj: &Self::Type) {
      self.parent_constructed(obj);
      obj.setup_gactions();
      obj.set_accels_for_action("app.quit", &["<primary>q"]);
    }
  }

  impl ApplicationImpl for OrcaApplication {
    fn activate(&self, application: &Self::Type) {
      let window = if let Some(window) = application.active_window() {
        window
      } else {
        let window = OrcaWindow::new(application);
        window.upcast()
      };
      window.present();
    }
  }

  impl GtkApplicationImpl for OrcaApplication {}
  impl AdwApplicationImpl for OrcaApplication {}
}

glib::wrapper! {
  pub struct OrcaApplication(ObjectSubclass<imp::OrcaApplication>)
  @extends gio::Application, gtk::Application, libadwaita::Application,
  @implements gio::ActionGroup, gio::ActionMap;
}

impl OrcaApplication {
  pub fn new(application_id: &str, flags: &gio::ApplicationFlags) -> Self {
    glib::Object::new(&[("application-id", &application_id), ("flags", flags)]).expect("Failed to create OrcaApplication")
  }

  fn setup_gactions(&self) {
    let quit_action = gio::SimpleAction::new("quit", None);
    quit_action.connect_activate(clone!(@weak self as app => move |_, _| {
      app.quit();
    }));

    let about_action = gio::SimpleAction::new("about", None);
    about_action.connect_activate(clone!(@weak self as app => move |_, _| {
      app.show_about();
    }));

    self.add_action(&quit_action);
    self.add_action(&about_action);
  }

  fn show_about(&self) {
    // TODO: https://gitlab.gnome.org/World/Rust/libadwaita-rs/-/merge_requests/42
    let window = self.active_window().unwrap();
    let dialog = gtk::AboutDialog::builder()
      .transient_for(&window)
      .modal(true)
      .program_name("orca")
      .version(VERSION)
      .authors(vec!["William Roy".into()])
      .build();
    dialog.present();
  }
}
