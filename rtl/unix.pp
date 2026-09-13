unit unix;

interface

uses baseunix;

function FpSystem(const Command: AnsiString): LongInt; external name '::u_unix::p_fpsystem';
function POpen(var F: Text; const Prog: AnsiString; rw: Char): LongInt; external name '::u_unix::p_popen';
function POpen(var F: File; const Prog: AnsiString; rw: Char): LongInt; external name '::u_unix::p_popen_file';
function PClose(var F: Text): LongInt; external name '::u_unix::p_pclose';
function PClose(var F: File): LongInt; external name '::u_unix::p_pclose_file';

implementation

end.
