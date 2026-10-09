with Ada.Text_IO; use Ada.Text_IO;
with Interfaces; use Interfaces;
with Interfaces.C.Strings;
with System;
with Hace.Cdr;
with Hace.Corba_Exceptions;
with Hace.Corba_Ior;
with Hace.Corba_Poa;
with Hace.Hal;
with Hace.Uiop;
with Example_Common;

package body Calculator_Impl is

   use type Hace.Cdr.Status;
   use type Hace.Corba_Poa.Status;
   use type Hace.Uiop.Status;

   Rpc_Basic_Port : constant := 59101;
   Calc_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('c')), Hace.Cdr.Octet (Character'Pos ('a')),
      Hace.Cdr.Octet (Character'Pos ('l')), Hace.Cdr.Octet (Character'Pos ('c')));

   procedure Calculator_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Calculator_Servant);

   procedure Calculator_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception)
   is
      pragma Unreferenced (Context);
      Op         : constant String := Interfaces.C.Strings.Value (Operation);
      A, B       : Interfaces.Integer_32;
      Cdr_Result : Hace.Cdr.Status;
   begin
      if Op /= "add" then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Bad_Operation;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Cdr.Decode_I32_At (Args, A, Cdr_Result);
      if Cdr_Result = Hace.Cdr.Ok then
         Hace.Cdr.Decode_I32_At (Args, B, Cdr_Result);
      end if;
      if Cdr_Result /= Hace.Cdr.Ok then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Marshal;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Cdr.Encode_I32_At (Reply, A + B, Cdr_Result);
      Put_Line ("server: add(" & A'Img & "," & B'Img & " ) =" & Integer_32'Image (A + B));
   end Calculator_Servant;

   procedure Run is
      Socket     : Hace.Hal.Socket_Handle;
      Ref        : Hace.Corba_Ior.Ior;
      Started    : Boolean;
      P          : Hace.Corba_Poa.Poa;
      Poa_Result : Hace.Corba_Poa.Status;
      T          : Hace.Uiop.Transport;
      Uiop_Result : Hace.Uiop.Status;
   begin
      Example_Common.Server_Start
        ("rpc_basic server", Rpc_Basic_Port, Calc_Object_Key, Socket, Ref, Started);
      if not Started then
         return;
      end if;

      Hace.Corba_Poa.Init (P, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "rpc_basic server: poa init failed");
         return;
      end if;

      Hace.Corba_Poa.Activate_Object
        (P, Calc_Object_Key, Calculator_Servant'Access, System.Null_Address, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "rpc_basic server: activate_object failed");
         return;
      end if;

      Hace.Uiop.For_Socket (T, Socket);

      --  Self-terminating (no Ctrl-C needed): handle exactly one call,
      --  matching this example's "one client makes one call" scope.
      Hace.Corba_Poa.Handle_One_Request (P, T, 10_000, Uiop_Result);
      if Uiop_Result = Hace.Uiop.Err_Timeout then
         Put_Line ("rpc_basic server: no request received within 10s, exiting");
      elsif Uiop_Result /= Hace.Uiop.Ok then
         Put_Line (Standard_Error, "rpc_basic server: dispatch error");
      end if;
   end Run;

end Calculator_Impl;
