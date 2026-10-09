--  observer_attach subject: activates a Subject object (object key
--  "subj") whose attach(obs) operation registers a caller-supplied
--  Observer object reference (CDR-encoded IOR, since there's no idlc
--  to marshal an object reference parameter automatically). Every
--  ~1s of wall-clock time, pushes one simulated event to every
--  attached observer via a oneway Observer.update() call -- the
--  server acting as a client on the very same socket it serves
--  attach() on. See examples/c/observer_attach/observer.idl for the
--  interfaces this hand-implements.

package Subject_Impl is
   procedure Run;
end Subject_Impl;
