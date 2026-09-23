//! hace-idlc: IDL compiler for hace-corba.
//!
//! Skeleton only. The lexer, parser, AST, semantic model, and per-language
//! backends (C, C++, Ada95, Rust) are planned for Phase 4 — see
//! docs/roadmap.md. This binary is a placeholder that establishes the
//! crate and its test setup ahead of that work; it is not part of the
//! DAL C certification boundary (see docs/architecture/adr-0001-runtime-language.md).

use std::env;
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("usage: hace-idlc <file.idl>");
        eprintln!(
            "not yet implemented: lexer/parser/backends land in Phase 4, see docs/roadmap.md"
        );
        return ExitCode::FAILURE;
    }

    eprintln!(
        "hace-idlc: parsing '{}' is not yet implemented (Phase 4, see docs/roadmap.md)",
        args[1]
    );
    ExitCode::FAILURE
}

#[cfg(test)]
mod tests {
    #[test]
    fn crate_builds_and_tests_run() {
        assert_eq!(2 + 2, 4);
    }
}
