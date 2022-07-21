use libadwaita::gio::compile_resources;

fn main() {
  compile_resources("resource", "resource/orca.gresource.xml", "orca.gresource");
}
