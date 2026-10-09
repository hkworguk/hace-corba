with Ada.Text_IO; use Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces.C.Strings;
with System;
with Hace.Cdr;
with Hace.Corba_Exceptions;
with Hace.Corba_Ior;
with Hace.Corba_Poa;
with Hace.Giop;
with Hace.Hal;
with Hace.Uiop;
with Example_Common;

package body Observer_Impl is

   use type Hace.Cdr.Status;
   use type Hace.Corba_Ior.Status;
   use type Hace.Corba_Poa.Status;
   use type Hace.Giop.Status;
   use type Hace.Uiop.Status;

   Observer_Port : constant := 59132;
   Subject_Port  : constant := 59131;
   Expected_Updates : constant := 3;
   Subject_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('s')), Hace.Cdr.Octet (Character'Pos ('u')),
      Hace.Cdr.Octet (Character'Pos ('b')), Hace.Cdr.Octet (Character'Pos ('j')));
   Observer_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('o')), Hace.Cdr.Octet (Character'Pos ('b')),
      Hace.Cdr.Octet (Character'Pos ('s')), Hace.Cdr.Octet (Character'Pos ('v')));

   type Counter_Access is access all Natural;
   function To_Counter_Access is new Ada.Unchecked_Conversion
     (System.Address, Counter_Access);

   procedure Update_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Update_Servant);

   procedure Update_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception)
   is
      pragma Unreferenced (Reply);
      Op         : constant String := Interfaces.C.Strings.Value (Operation);
      Event      : String (1 .. 128);
      Last       : Natural;
      Cdr_Result : Hace.Cdr.Status;
      Counter    : constant Counter_Access := To_Counter_Access (Context);
   begin
      if Op /= "update" then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Bad_Operation;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Cdr.Decode_String_At (Args, Event, Last, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Marshal;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Put_Line ("observer: received update: " & Event (Event'First .. Last));
      Counter.all := Counter.all + 1;
   end Update_Servant;

   procedure Run is
      Subject_Ior      : constant Hace.Corba_Ior.Ior :=
        Example_Common.Parse_Ior_Arg (Subject_Port, Subject_Object_Key);

      Socket       : Hace.Hal.Socket_Handle;
      Self_Ior     : Hace.Corba_Ior.Ior;
      Started      : Boolean;
      Received     : aliased Natural := 0;
      P            : Hace.Corba_Poa.Poa;
      Poa_Result   : Hace.Corba_Poa.Status;
      T            : Hace.Uiop.Transport;
      Uiop_Result  : Hace.Uiop.Status;

      Storage      : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
      Request_Buf  : Hace.Cdr.Buffer;
      Cdr_Result   : Hace.Cdr.Status;
      Giop_Result  : Hace.Giop.Status;
      Ior_Result   : Hace.Corba_Ior.Status;

      Reply_Body   : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
      Reply_Last   : Integer;
      Is_Exception : Boolean;
   begin
      --  The observer is itself a tiny server: it needs a known,
      --  stable address for the subject to call update() back on.
      Example_Common.Server_Start
        ("observer_attach observer", Observer_Port, Observer_Object_Key, Socket,
         Self_Ior, Started);
      if not Started then
         return;
      end if;

      Hace.Corba_Poa.Init (P, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "observer: poa init failed");
         return;
      end if;

      Hace.Corba_Poa.Activate_Object
        (P, Observer_Object_Key, Update_Servant'Access, Received'Address, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "observer: activate_object failed");
         return;
      end if;

      Hace.Uiop.For_Socket (T, Socket);

      Hace.Cdr.Init (Request_Buf, Storage, Hace.Cdr.Big_Endian, Cdr_Result);
      Hace.Giop.Begin_Message (Request_Buf, Hace.Cdr.Big_Endian, Giop_Result);
      if Giop_Result = Hace.Giop.Ok then
         Hace.Giop.Encode_Request_Header
           (Request_Buf, 1, True, Subject_Object_Key, "attach", Giop_Result);
      end if;
      if Giop_Result = Hace.Giop.Ok then
         Hace.Corba_Ior.Encode (Request_Buf, Self_Ior, Ior_Result);
         if Ior_Result /= Hace.Corba_Ior.Ok then
            Giop_Result := Hace.Giop.Err_Buffer_Overflow;
         end if;
      end if;
      if Giop_Result = Hace.Giop.Ok then
         Hace.Giop.Finish_Message (Request_Buf, Giop_Result);
      end if;
      if Giop_Result /= Hace.Giop.Ok then
         Put_Line (Standard_Error, "observer: could not encode attach request");
         return;
      end if;

      Hace.Uiop.Call
        (T, Request_Buf, 1, Subject_Ior.Host_Addr, Subject_Ior.Port, 1000, 3,
         Reply_Body, Reply_Last, Is_Exception, Uiop_Result);
      if Uiop_Result /= Hace.Uiop.Ok then
         Put_Line (Standard_Error, "observer: attach call failed");
         return;
      end if;
      if Is_Exception then
         Put_Line (Standard_Error, "observer: attach was rejected by the subject");
         return;
      end if;
      Put_Line ("observer: attached to subject");

      while Received < Expected_Updates loop
         Hace.Corba_Poa.Handle_One_Request (P, T, 5000, Uiop_Result);
         if Uiop_Result = Hace.Uiop.Err_Timeout then
            Put_Line ("observer: no update received within 5s, exiting");
            exit;
         elsif Uiop_Result /= Hace.Uiop.Ok then
            Put_Line (Standard_Error, "observer: dispatch error");
            exit;
         end if;
      end loop;
   end Run;

end Observer_Impl;
