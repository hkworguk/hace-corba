--  observer_attach observer: runs its own POA on its own socket
--  (object key "obsv") so it can receive callbacks, calls
--  Subject.attach(self) on the subject via a real twoway
--  Hace.Uiop.Call (passing its own IOR, CDR-encoded, as the request
--  argument), then dispatches incoming oneway Observer.update() calls
--  on that same local POA -- the same mechanism the subject uses to
--  serve attach(), just acting as client for that one call and server
--  for the rest. See examples/c/observer_attach/observer.idl.

package Observer_Impl is
   procedure Run;
end Observer_Impl;
