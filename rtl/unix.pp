unit unix;

interface

function FpSystem(const Command: AnsiString): LongInt; external name '::u_system::p_fpsystem';

implementation

end.
