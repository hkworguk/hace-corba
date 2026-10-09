--  rpc_basic server implementation: activates a Calculator object
--  (object key "calc") whose add(a, b) operation returns a + b, over
--  a blocking twoway call. See examples/c/rpc_basic/calculator.idl
--  for the interface this hand-implements (there is no idlc yet --
--  Phase 4).
--
--  The servant procedure is declared at this package's library level
--  (not nested inside Run) because Ada forbids taking 'Access of a
--  subprogram nested deeper than the access type it's assigned to
--  (Hace.Corba_Poa.Servant_Procedure is declared at Hace.Corba_Poa's
--  library level) -- see docs/architecture/adr-0005-rust-ada-orb-bindings.md.

package Calculator_Impl is
   procedure Run;
end Calculator_Impl;
