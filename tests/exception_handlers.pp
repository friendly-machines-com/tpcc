program ExceptionHandlers;

uses
  SysUtils;

{$R+}

label
  RetryInsideTry;

type
  EBase = class(Exception);
  EChild = class(EBase);
  EOther = class(Exception);
  ETracked = class(Exception)
    destructor Destroy; override;
  end;

var
  State: LongInt;
  Destroyed: LongInt;
  Address: Pointer;
  SavedException: Exception;
  OneCharacter: string[1];

destructor ETracked.Destroy;
begin
  Destroyed := Destroyed + 1
end;

begin
  State := 0;
  try
    raise EChild.Create('child')
  except
    on E: EChild do
      if E.Message = 'child' then
        State := 1
  end;
  if State <> 1 then
    Halt(1);

  State := 0;
  try
    raise EChild.Create('ordered')
  except
    on EBase do
      State := 2;
    on EChild do
      State := 99
  end;
  if State <> 2 then
    Halt(2);

  State := 0;
  try
    raise EOther.Create('other')
  except
    on EChild do
      State := 99
    else
      State := 3
  end;
  if State <> 3 then
    Halt(3);

  State := 0;
  try
    raise EOther.Create('qualified')
  except
    on SysUtils.Exception do
      State := 31
  end;
  if State <> 31 then
    Halt(31);

  State := 0;
  try
    try
      raise EChild.Create('again')
    except
      on EChild do
        raise
    end
  except
    on EBase do
      State := 4
  end;
  if State <> 4 then
    Halt(4);

  Destroyed := 0;
  try
    raise ETracked.Create('tracked')
  except
    on ETracked do
      State := 5
  end;
  if Destroyed <> 1 then
    Halt(5);

  Destroyed := 0;
  State := 0;
  try
    try
      raise ETracked.Create('explicit re-raise')
    except
      on E: ETracked do
        begin
          SavedException := E;
          raise SavedException
        end
    end
  except
    on ETracked do
      State := 51
  end;
  if State <> 51 then
    Halt(51);
  if Destroyed <> 1 then
    Halt(52);

  Address := nil;
  try
    raise EChild.Create('addressed') at Address, Address
  except
    on EChild do
      State := 6
  end;
  if State <> 6 then
    Halt(6);

  try
    OneCharacter[2] := 'x'
  except
    on ERangeError do
      State := 7
  end;
  if State <> 7 then
    Halt(7);

  State := 0;
  try
  RetryInsideTry:
    State := State + 1;
    if State < 2 then
      goto RetryInsideTry
  finally
    State := State + 1
  end;
  if State <> 3 then
    Halt(8)
end.
