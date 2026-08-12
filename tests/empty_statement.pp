program EmptyStatement;

label
  EmptyAtEnd;

type
  TState = record
    Value: LongInt
  end;

procedure CompileOnly;
begin
  while True do
    ;
end;

var
  I, N: LongInt;
  State: TState;

begin;
  ;;
  N := 0;

  case N of
    0:
      ;
  else
    N := 1 div N
  end;

  if False then
  else
    N := N + 1;

  if True then;

  if False then
  else
    ;

  while False do
    ;

  for I := 1 to 0 do
    ;

  for I in [1] do
    ;

  with State do
    ;

  if True then
    begin
      if True then
    end;

  repeat
    ;
  until True;

  try
    ;
  finally
    ;
  end;

  if N <> 1 then
    begin
      N := 0;
      N := 1 div N
    end;

EmptyAtEnd:
end.
