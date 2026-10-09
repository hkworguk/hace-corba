package body Hace.Corba_Poa is

   use type Interfaces.C.size_t;

   function C_Poa_Sizeof return Interfaces.C.size_t;
   pragma Import (C, C_Poa_Sizeof, "hace_corba_poa_sizeof");

   function C_Poa_Init (P : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Poa_Init, "hace_corba_poa_init");

   function C_Poa_Activate_Object
     (P                 : System.Address;
      Object_Key        : System.Address;
      Object_Key_Length : Interfaces.C.size_t;
      Servant           : Servant_Procedure;
      Context           : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Poa_Activate_Object, "hace_corba_poa_activate_object");

   function C_Poa_Handle_One_Request
     (P            : System.Address;
      Transport    : System.Address;
      Timeout_Ms   : Interfaces.Unsigned_32;
      Max_Datagram : Interfaces.C.size_t) return Interfaces.C.int;
   pragma Import (C, C_Poa_Handle_One_Request, "hace_corba_poa_handle_one_request");

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
              " -- the Ada binding is out of sync with runtime/include/hace/corba_poa.h";
      end case;
   end To_Status;

   function To_Uiop_Status (Raw : Interfaces.C.int) return Hace.Uiop.Status is
   begin
      case Raw is
         when 0 => return Hace.Uiop.Ok;
         when 1 => return Hace.Uiop.Err_Invalid_Argument;
         when 2 => return Hace.Uiop.Err_Hal;
         when 3 => return Hace.Uiop.Err_Too_Large;
         when 4 => return Hace.Uiop.Err_Timeout;
         when others =>
            raise Program_Error with
              "hace_uiop_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/include/hace/uiop.h";
      end case;
   end To_Uiop_Status;

   procedure Init (P : out Poa; Result : out Status) is
      Native_Size : constant Interfaces.C.size_t := C_Poa_Sizeof;
   begin
      if Native_Size > Interfaces.C.size_t (Opaque_Size) then
         raise Poa_Sizeof_Mismatch with
           "hace_corba_poa_t (" & Native_Size'Img &
           " bytes) no longer fits the Ada binding's opaque storage (" &
           Opaque_Size'Img & " bytes) -- update Opaque_Size in hace-corba_poa.ads";
      end if;

      P.Raw := (others => 0);
      Result := To_Status (C_Poa_Init (P.Raw'Address));
   end Init;

   procedure Activate_Object
     (P          : in out Poa;
      Object_Key : Hace.Cdr.Byte_Array;
      Servant    : Servant_Procedure;
      Context    : System.Address;
      Result     : out Status)
   is
      Key_Storage : array (0 .. Object_Key'Length - 1) of Interfaces.C.unsigned_char;
   begin
      for I in Object_Key'Range loop
         Key_Storage (I - Object_Key'First) := Interfaces.C.unsigned_char (Object_Key (I));
      end loop;
      Result :=
        To_Status
          (C_Poa_Activate_Object
             (P.Raw'Address, Key_Storage (Key_Storage'First)'Address,
              Interfaces.C.size_t (Object_Key'Length), Servant, Context));
   end Activate_Object;

   procedure Handle_One_Request
     (P          : Poa;
      T          : Hace.Uiop.Transport;
      Timeout_Ms : Interfaces.Unsigned_32;
      Result     : out Hace.Uiop.Status)
   is
   begin
      Result :=
        To_Uiop_Status
          (C_Poa_Handle_One_Request
             (P.Raw'Address, Hace.Uiop.Raw_Address (T), Timeout_Ms,
              Hace.Uiop.Max_Datagram_Size));
   end Handle_One_Request;

end Hace.Corba_Poa;
