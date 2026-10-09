with Ada.Text_IO; use Ada.Text_IO;
with Interfaces.C.Strings;
with System;
with Hace.Cdr;
with Hace.Corba_Exceptions;
with Hace.Corba_Ior;
with Hace.Corba_Poa;
with Hace.Hal;
with Hace.Uiop;
with Example_Common;

package body Logger_Impl is

   use type Hace.Cdr.Status;
   use type Hace.Corba_Poa.Status;
   use type Hace.Uiop.Status;

   Oneway_Port : constant := 59111;
   Log_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 3) :=
     (Hace.Cdr.Octet (Character'Pos ('l')), Hace.Cdr.Octet (Character'Pos ('o')),
      Hace.Cdr.Octet (Character'Pos ('g')));

   procedure Logger_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Logger_Servant);

   procedure Logger_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception)
   is
      pragma Unreferenced (Context, Reply);
      Op         : constant String := Interfaces.C.Strings.Value (Operation);
      Message    : String (1 .. 128);
      Last       : Natural;
      Cdr_Result : Hace.Cdr.Status;
   begin
      if Op /= "log" then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Bad_Operation;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Cdr.Decode_String_At (Args, Message, Last, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Marshal;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Put_Line ("server: log: " & Message (Message'First .. Last));
   end Logger_Servant;

   procedure Run is
      Socket      : Hace.Hal.Socket_Handle;
      Ref         : Hace.Corba_Ior.Ior;
      Started     : Boolean;
      P           : Hace.Corba_Poa.Poa;
      Poa_Result  : Hace.Corba_Poa.Status;
      T           : Hace.Uiop.Transport;
      Uiop_Result : Hace.Uiop.Status;
      Handled     : Natural := 0;
   begin
      Example_Common.Server_Start
        ("oneway server", Oneway_Port, Log_Object_Key, Socket, Ref, Started);
      if not Started then
         return;
      end if;

      Hace.Corba_Poa.Init (P, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "oneway server: poa init failed");
         return;
      end if;

      Hace.Corba_Poa.Activate_Object
        (P, Log_Object_Key, Logger_Servant'Access, System.Null_Address, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "oneway server: activate_object failed");
         return;
      end if;

      Hace.Uiop.For_Socket (T, Socket);

      --  Self-terminating: handle up to 3 fire-and-forget messages
      --  (matching client.adb's three log() calls), or stop once 5s
      --  passes without one.
      while Handled < 3 loop
         Hace.Corba_Poa.Handle_One_Request (P, T, 5000, Uiop_Result);
         if Uiop_Result = Hace.Uiop.Ok then
            Handled := Handled + 1;
         elsif Uiop_Result = Hace.Uiop.Err_Timeout then
            Put_Line ("oneway server: no more messages within 5s, exiting");
            exit;
         else
            Put_Line (Standard_Error, "oneway server: dispatch error");
            exit;
         end if;
      end loop;
   end Run;

end Logger_Impl;
