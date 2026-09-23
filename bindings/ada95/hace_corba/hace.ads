--  Root package for the hace-corba Ada95 bindings.
--
--  These bindings call into the certified C runtime (runtime/) via
--  Interfaces.C. They are not part of the DAL C certification boundary
--  themselves -- see docs/architecture/adr-0001-runtime-language.md.
package Hace is
   pragma Pure;
end Hace;
