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

package body Worker_Impl is

   use type Hace.Cdr.Status;
   use type Hace.Corba_Poa.Status;
   use type Hace.Uiop.Status;

   Async_Ami_Port : constant := 59121;
   Work_Simulated_Seconds : constant Duration := 0.3;
   Work_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('w')), Hace.Cdr.Octet (Character'Pos ('o')),
      Hace.Cdr.Octet (Character'Pos ('r')), Hace.Cdr.Octet (Character'Pos ('k')));

   procedure Worker_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Worker_Servant);

   procedure Worker_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception)
   is
      pragma Unreferenced (Context);
      Op         : constant String := Interfaces.C.Strings.Value (Operation);
      Input      : Interfaces.Integer_32;
      Cdr_Result : Hace.Cdr.Status;
   begin
      if Op /= "compute" then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Bad_Operation;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Cdr.Decode_I32_At (Args, Input, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Marshal;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Put_Line
        ("server: computing for input=" & Input'Img & " (simulating 300ms of work)...");
      delay Work_Simulated_Seconds;

      Hace.Cdr.Encode_I32_At (Reply, Input * Input, Cdr_Result);
      Put_Line ("server: compute(" & Input'Img & " ) =" & Integer_32'Image (Input * Input));
   end Worker_Servant;

   procedure Run is
      Socket      : Hace.Hal.Socket_Handle;
      Ref         : Hace.Corba_Ior.Ior;
      Started     : Boolean;
      P           : Hace.Corba_Poa.Poa;
      Poa_Result  : Hace.Corba_Poa.Status;
      T           : Hace.Uiop.Transport;
      Uiop_Result : Hace.Uiop.Status;
   begin
      Example_Common.Server_Start
        ("async_ami server", Async_Ami_Port, Work_Object_Key, Socket, Ref, Started);
      if not Started then
         return;
      end if;

      Hace.Corba_Poa.Init (P, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "async_ami server: poa init failed");
         return;
      end if;

      Hace.Corba_Poa.Activate_Object
        (P, Work_Object_Key, Worker_Servant'Access, System.Null_Address, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "async_ami server: activate_object failed");
         return;
      end if;

      Hace.Uiop.For_Socket (T, Socket);

      Hace.Corba_Poa.Handle_One_Request (P, T, 10_000, Uiop_Result);
      if Uiop_Result = Hace.Uiop.Err_Timeout then
         Put_Line ("async_ami server: no request received within 10s, exiting");
      elsif Uiop_Result /= Hace.Uiop.Ok then
         Put_Line (Standard_Error, "async_ami server: dispatch error");
      end if;
   end Run;

end Worker_Impl;
