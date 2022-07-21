// window.rs
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

use gtk::gio;
use gtk::glib;
use gtk::prelude::*;
use gtk::subclass::prelude::*;
use gtk::CompositeTemplate;

use libadwaita::subclass::prelude::AdwApplicationWindowImpl;
use libadwaita::ApplicationWindow;

mod imp {
  use super::*;

  #[derive(Debug, Default, CompositeTemplate)]
  #[template(resource = "/com/github/wroyca/orca/window.ui")]
  pub struct OrcaWindow {
    #[template_child]
    pub header_bar: TemplateChild<gtk::HeaderBar>,
    #[template_child]
    pub label: TemplateChild<gtk::Label>
  }

  #[glib::object_subclass]
  impl ObjectSubclass for OrcaWindow {
    type ParentType = ApplicationWindow;
    type Type = super::OrcaWindow;

    const NAME: &'static str = "OrcaWindow";

    fn class_init(klass: &mut Self::Class) {
      Self::bind_template(klass);
    }

    fn instance_init(obj: &glib::subclass::InitializingObject<Self>) {
      obj.init_template();
    }
  }

  impl ObjectImpl for OrcaWindow {}
  impl WidgetImpl for OrcaWindow {}
  impl WindowImpl for OrcaWindow {}
  impl ApplicationWindowImpl for OrcaWindow {}
  impl AdwApplicationWindowImpl for OrcaWindow {}
}

glib::wrapper! {
  pub struct OrcaWindow(ObjectSubclass<imp::OrcaWindow>)
  @extends gtk::Widget, gtk::Window, ApplicationWindow,
  @implements gio::ActionGroup, gio::ActionMap;
}

impl OrcaWindow {
  pub fn new<P: glib::IsA<gtk::Application>>(application: &P) -> Self {
    glib::Object::new(&[("application", application)]).expect("Failed to create OrcaWindow")
  }
}
