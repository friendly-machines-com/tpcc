unit baseunix;

interface

{ The returned PChar aliases the process environment.  It is not an allocated
  Pascal string and remains owned by the C environment implementation. }
function FpGetEnv(Name: PChar): PChar; external name '::u_system::p_fpgetenv';

implementation
end.
