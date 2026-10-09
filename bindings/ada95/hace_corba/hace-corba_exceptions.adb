with Interfaces.C;
with System;

package body Hace.Corba_Exceptions is

   function C_Decode_System_Exception
     (Buf : System.Address; Out_Exc : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Decode_System_Exception, "hace_corba_decode_system_exception");

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err_Invalid_Argument;
         when 2 => return Err_Buffer_Overflow;
         when 3 => return Err_Buffer_Underflow;
         when others =>
            raise Program_Error with
              "hace_corba_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with " &
              "runtime/include/hace/corba_exception.h";
      end case;
   end To_Status;

   procedure Decode_System_Exception
     (Buf : in out Hace.Cdr.Buffer; Exc : out System_Exception; Result : out Status)
   is
   begin
      Result :=
        To_Status (C_Decode_System_Exception (Hace.Cdr.Raw_Address (Buf), Exc'Address));
   end Decode_System_Exception;

end Hace.Corba_Exceptions;
