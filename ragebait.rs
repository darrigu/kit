use std::ffi::CString;
use std::os::raw::{c_char, c_int};
use std::ptr;

#[link(name = "kit")]
extern "C" {
    fn kit_auto_rebuild(
        argc: c_int,
        argv: *mut *mut c_char,
        source_file: *const c_char,
        cc_template: *const c_char,
    ) -> bool;
}

fn main() {
    let source_c = CString::new(file!()).unwrap();
    let tpl_c = CString::new("rustc -L. -o %s %s").unwrap();

    let arg_cstrings: Vec<CString> = std::env::args().map(|a| CString::new(a).unwrap()).collect();
    let mut argv_ptrs: Vec<*mut c_char> = arg_cstrings
        .iter()
        .map(|cs| cs.as_ptr() as *mut c_char)
        .collect();
    argv_ptrs.push(ptr::null_mut());

    unsafe {
        kit_auto_rebuild(
            argv_ptrs.len() as c_int - 1,
            argv_ptrs.as_mut_ptr(),
            source_c.as_ptr(),
            tpl_c.as_ptr(),
        );
    }

    println!("Hello, World!");
}
