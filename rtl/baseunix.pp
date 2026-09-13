unit baseunix;

interface

type
  SignalHandler = procedure(Sig: LongInt);

const
  SIGINT = 2;

{ The returned PChar aliases the process environment.  It is not an allocated
  Pascal string and remains owned by the C environment implementation. }
function FpGetEnv(Name: PChar): PChar; external name '::u_baseunix::p_fpgetenv';
function FpChmod(path: AnsiString; Mode: LongInt): LongInt; external name '::u_baseunix::p_fpchmod';
function FpSignal(signum: LongInt; Handler: SignalHandler): SignalHandler; external name '::u_baseunix::p_fpsignal';

implementation
end.
