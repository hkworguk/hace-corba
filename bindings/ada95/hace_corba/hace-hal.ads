--  Ada95 bindings for the hace-corba HAL (runtime/src/hal/hal.h):
--  monotonic time and UDP sockets only -- task/thread creation and the
--  memory pool are runtime-internal plumbing, not meant for application
--  code, so they are not bound here (matches
--  bindings/rust/hace-corba/src/hal.rs's scope). Not part of the DAL C
--  certification boundary -- see docs/architecture/adr-0001-runtime-language.md.

with Ada.Streams;
with Interfaces;
with Interfaces.C;

package Hace.Hal is

   type Status is
     (Ok,
      Err,
      Err_Timeout,
      Err_Would_Block,
      Err_Invalid_Argument,
      Err_Resource_Exhausted,
      Err_Not_Implemented);

   type Socket_Handle is private;
   Invalid_Socket : constant Socket_Handle;

   --  Numeric IPv4 form, e.g. 127.0.0.1 = 16#7F000001#, matching
   --  hal.h's dest_addr_ipv4/src_addr_ipv4 (host-order numeric address,
   --  the same convention as Rust's std::net::Ipv4Addr <-> u32).
   type Ipv4_Address is new Interfaces.Unsigned_32;

   type Monotonic_Time is record
      Seconds     : Interfaces.Unsigned_64;
      Nanoseconds : Interfaces.Unsigned_32;
   end record;

   --  Monotonic clock reading -- for retry/timeout scheduling, never a
   --  calendar timestamp.
   procedure Now (Time : out Monotonic_Time; Result : out Status);

   --  Opens a UDP socket bound to Local_Port (0 = backend-chosen
   --  ephemeral port).
   procedure Open
     (Handle     : out Socket_Handle;
      Local_Port : Interfaces.Unsigned_16;
      Result     : out Status);

   procedure Close (Handle : in out Socket_Handle; Result : out Status);

   --  Exposes Handle's underlying C int value to Hace.Uiop.For_Socket,
   --  which copies it into its own Transport record and passes that
   --  copy's address to hace_uiop_transport_for_socket (the production
   --  transport dereferences its context pointer on every send/recv,
   --  so it must point at storage that outlives the call -- the
   --  Transport object itself, not a transient copy of Handle).
   function To_Raw (Handle : Socket_Handle) return Interfaces.C.int;

   procedure Send
     (Handle    : Socket_Handle;
      Data      : Ada.Streams.Stream_Element_Array;
      Dest_Addr : Ipv4_Address;
      Dest_Port : Interfaces.Unsigned_16;
      Result    : out Status);

   --  Blocks up to Timeout_Ms for a datagram (0 = poll, returns
   --  Err_Would_Block if nothing is pending). A datagram larger than
   --  Data is rejected (Err_Invalid_Argument), never silently
   --  truncated -- Last/Src_Addr/Src_Port are only meaningful when
   --  Result = Ok.
   procedure Receive
     (Handle     : Socket_Handle;
      Data       : out Ada.Streams.Stream_Element_Array;
      Last       : out Ada.Streams.Stream_Element_Offset;
      Src_Addr   : out Ipv4_Address;
      Src_Port   : out Interfaces.Unsigned_16;
      Timeout_Ms : Interfaces.Unsigned_32;
      Result     : out Status);

private
   type Socket_Handle is new Interfaces.Integer_32;
   Invalid_Socket : constant Socket_Handle := -1;
end Hace.Hal;
