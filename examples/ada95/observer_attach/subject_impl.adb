with Ada.Text_IO; use Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces; use Interfaces;
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

package body Subject_Impl is

   use type Hace.Cdr.Status;
   use type Hace.Corba_Ior.Status;
   use type Hace.Corba_Poa.Status;
   use type Hace.Giop.Status;
   use type Hace.Hal.Status;
   use type Hace.Uiop.Status;

   Subject_Port       : constant := 59131;
   Notify_Count        : constant := 3;
   Notify_Interval_Ms   : constant := 1000;
   Max_Attached        : constant := 4;
   Subject_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('s')), Hace.Cdr.Octet (Character'Pos ('u')),
      Hace.Cdr.Octet (Character'Pos ('b')), Hace.Cdr.Octet (Character'Pos ('j')));

   type Observer_Slots is array (1 .. Max_Attached) of Hace.Corba_Ior.Ior;
   type In_Use_Slots is array (1 .. Max_Attached) of Boolean;

   type Observer_Registry is record
      Observers : Observer_Slots;
      In_Use    : In_Use_Slots := (others => False);
      Count     : Natural := 0;
   end record;

   type Registry_Access is access all Observer_Registry;
   function To_Registry_Access is new Ada.Unchecked_Conversion
     (System.Address, Registry_Access);

   procedure Attach_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Attach_Servant);

   procedure Attach_Servant
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception)
   is
      pragma Unreferenced (Reply);
      Op           : constant String := Interfaces.C.Strings.Value (Operation);
      Observer_Ior : Hace.Corba_Ior.Ior;
      Ior_Result   : Hace.Corba_Ior.Status;
      Registry     : constant Registry_Access := To_Registry_Access (Context);
      Slot         : Natural := 0;
   begin
      if Op /= "attach" then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Bad_Operation;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Hace.Corba_Ior.Decode_At (Args, Observer_Ior, Ior_Result);
      if Ior_Result /= Hace.Corba_Ior.Ok then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.Marshal;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      for I in Registry.In_Use'Range loop
         if not Registry.In_Use (I) then
            Slot := I;
            exit;
         end if;
      end loop;
      if Slot = 0 then
         Out_Exception.Exception_Value := Hace.Corba_Exceptions.No_Memory;
         Out_Exception.Completion := Hace.Corba_Exceptions.Completed_No;
         return;
      end if;

      Registry.Observers (Slot) := Observer_Ior;
      Registry.In_Use (Slot) := True;
      Registry.Count := Registry.Count + 1;
      Put_Line
        ("subject: observer attached (slot" & Slot'Img & "), total=" &
         Registry.Count'Img);
   end Attach_Servant;

   function Time_To_Ms (T : Hace.Hal.Monotonic_Time) return Long_Long_Integer is
   begin
      return
        Long_Long_Integer (T.Seconds) * 1000 +
        Long_Long_Integer (T.Nanoseconds) / 1_000_000;
   end Time_To_Ms;

   procedure Build_Update_Request
     (Buf        : in out Hace.Cdr.Buffer;
      Request_Id : Interfaces.Unsigned_32;
      Event      : String;
      Object_Key : Hace.Cdr.Byte_Array;
      Result     : out Hace.Giop.Status)
   is
      Cdr_Result : Hace.Cdr.Status;
   begin
      Hace.Giop.Begin_Message (Buf, Hace.Cdr.Big_Endian, Result);
      if Result = Hace.Giop.Ok then
         Hace.Giop.Encode_Request_Header
           (Buf, Request_Id, False, Object_Key, "update", Result);
      end if;
      if Result = Hace.Giop.Ok then
         Hace.Cdr.Encode_String (Buf, Event, Cdr_Result);
         if Cdr_Result /= Hace.Cdr.Ok then
            Result := Hace.Giop.Err_Buffer_Overflow;
            return;
         end if;
      end if;
      if Result = Hace.Giop.Ok then
         Hace.Giop.Finish_Message (Buf, Result);
      end if;
   end Build_Update_Request;

   procedure Notify_All
     (Registry        : Observer_Registry;
      T               : Hace.Uiop.Transport;
      Next_Request_Id : in out Interfaces.Unsigned_32;
      Event           : String)
   is
   begin
      for I in Registry.In_Use'Range loop
         if Registry.In_Use (I) then
            declare
               Id          : constant Interfaces.Unsigned_32 := Next_Request_Id;
               Storage     : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
               Buf         : Hace.Cdr.Buffer;
               Cdr_Result  : Hace.Cdr.Status;
               Giop_Result : Hace.Giop.Status;
               Uiop_Result : Hace.Uiop.Status;
            begin
               Next_Request_Id := Next_Request_Id + 1;
               Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Cdr_Result);
               Build_Update_Request
                 (Buf, Id, Event,
                  Registry.Observers (I).Object_Key
                    (Registry.Observers (I).Object_Key'First ..
                       Registry.Observers (I).Object_Key'First +
                       Registry.Observers (I).Object_Key_Length - 1),
                  Giop_Result);
               if Giop_Result = Hace.Giop.Ok then
                  Hace.Uiop.Send_Message
                    (T, Buf, Id, Registry.Observers (I).Host_Addr,
                     Registry.Observers (I).Port, Uiop_Result);
               end if;
               if Giop_Result = Hace.Giop.Ok and then Uiop_Result = Hace.Uiop.Ok then
                  Put_Line ("subject: notified observer" & I'Img & ": " & Event);
               else
                  Put_Line (Standard_Error, "subject: failed to notify observer" & I'Img);
               end if;
            end;
         end if;
      end loop;
   end Notify_All;

   type Event_Array is array (1 .. Notify_Count) of access constant String;
   Events : constant Event_Array :=
     (new String'("temperature=21C"), new String'("temperature=23C"),
      new String'("temperature=19C"));

   procedure Run is
      Socket           : Hace.Hal.Socket_Handle;
      Ref              : Hace.Corba_Ior.Ior;
      Started          : Boolean;
      Registry         : aliased Observer_Registry;
      P                : Hace.Corba_Poa.Poa;
      Poa_Result       : Hace.Corba_Poa.Status;
      T                : Hace.Uiop.Transport;
      Uiop_Result      : Hace.Uiop.Status;
      Start_Time       : Hace.Hal.Monotonic_Time;
      Now_Time         : Hace.Hal.Monotonic_Time;
      Hal_Result       : Hace.Hal.Status;
      Start_Ms         : Long_Long_Integer;
      Next_Request_Id  : Interfaces.Unsigned_32 := 1000; -- distinct from clients' own ids
      Notifications_Sent : Natural := 0;
   begin
      Example_Common.Server_Start
        ("observer_attach subject", Subject_Port, Subject_Object_Key, Socket, Ref,
         Started);
      if not Started then
         return;
      end if;

      Hace.Corba_Poa.Init (P, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "subject: poa init failed");
         return;
      end if;

      Hace.Corba_Poa.Activate_Object
        (P, Subject_Object_Key, Attach_Servant'Access, Registry'Address, Poa_Result);
      if Poa_Result /= Hace.Corba_Poa.Ok then
         Put_Line (Standard_Error, "subject: activate_object failed");
         return;
      end if;

      Hace.Uiop.For_Socket (T, Socket);

      Hace.Hal.Now (Start_Time, Hal_Result);
      if Hal_Result /= Hace.Hal.Ok then
         Put_Line (Standard_Error, "subject: could not read the clock");
         return;
      end if;
      Start_Ms := Time_To_Ms (Start_Time);

      while Notifications_Sent < Notify_Count loop
         Hace.Corba_Poa.Handle_One_Request (P, T, 200, Uiop_Result);
         if Uiop_Result /= Hace.Uiop.Ok and Uiop_Result /= Hace.Uiop.Err_Timeout then
            Put_Line (Standard_Error, "subject: dispatch error");
            exit;
         end if;

         Hace.Hal.Now (Now_Time, Hal_Result);
         exit when Hal_Result /= Hace.Hal.Ok;

         if (Time_To_Ms (Now_Time) - Start_Ms) >=
           Long_Long_Integer (Notifications_Sent + 1) * Notify_Interval_Ms
         then
            Notify_All
              (Registry, T, Next_Request_Id, Events (Notifications_Sent + 1).all);
            Notifications_Sent := Notifications_Sent + 1;
         end if;
      end loop;
   end Run;

end Subject_Impl;
