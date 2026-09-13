program FreeAndNilTest;
{$mode objfpc}
uses SysUtils;
type
  TObserved = class
    destructor Destroy; override;
  end;
  TRaising = class
    destructor Destroy; override;
  end;
  EDestructorFailure = class(Exception);
var
  Observed: TObserved;
  Raising: TRaising;
  Destroyed, Raised: Integer;
  SawNil, Caught: Boolean;

destructor TObserved.Destroy;
begin
  if Observed <> nil then Halt(11);
  Inc(Destroyed);
  { Re-entering FreeAndNil must do nothing: the slot is already nil. }
  FreeAndNil(Observed);
  inherited Destroy
end;

destructor TRaising.Destroy;
begin
  SawNil := Raising = nil;
  if not SawNil then Halt(12);
  Inc(Raised);
  inherited Destroy;
  raise EDestructorFailure.Create('destructor marker')
end;

begin
  case ParamStr(1) of
    'nil':
      begin
        Observed := nil;
        FreeAndNil(Observed);
        if (Observed <> nil) or (Destroyed <> 0) then Halt(1)
      end;
    'normal':
      begin
        Observed := TObserved.Create;
        FreeAndNil(Observed);
        if (Observed <> nil) or (Destroyed <> 1) then Halt(2);
        FreeAndNil(Observed);
        if Destroyed <> 1 then Halt(3)
      end;
    'raising':
      begin
        Raising := TRaising.Create;
        try
          FreeAndNil(Raising);
          Halt(4)
        except
          on E: EDestructorFailure do
            begin
              Caught := True;
              if E.Message <> 'destructor marker' then Halt(5);
              if (Raising <> nil) or not SawNil then Halt(6)
            end
        end;
        if not Caught or (Raised <> 1) then Halt(7);
        FreeAndNil(Raising);
        if Raised <> 1 then Halt(8)
      end
  else Halt(99)
  end;
  WriteLn('freeandnil ok')
end.
