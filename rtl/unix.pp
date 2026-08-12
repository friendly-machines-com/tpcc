unit unix;

interface

function FpSystem(const Command: AnsiString): LongInt; external name '::u_unix::p_fpsystem';

implementation

end.
