program TypeBlockPublication;

type
  TBase = class
  public
    Value: LongInt;
    procedure SetValue(NewValue: LongInt);
    function GetValue: LongInt; virtual;
  end;
  TChild = class(TBase)
  public
    procedure UseInheritedMembers;
    function GetValue: LongInt; override;
  end;

  TForward = class;

  TForwardClass = class of TForward;

  TForwardUser = class
  public
    Ref: TForward;
    Meta: TForwardClass;
  end;

var
  ForwardGlobal: TForward;

type
  TForward = class
  public
    User: TForwardUser;
  end;

  TLeftForward = class;
  TRightForward = class;
  TLeftForwardClass = class of TLeftForward;
  TRightForwardClass = class of TRightForward;

  TLeftForward = class
  public
    RightClass: TRightForwardClass;
  end;

  TRightForward = class
  public
    LeftClass: TLeftForwardClass;
  end;

{$ifdef TEST_UNRESOLVED_CLASS_FORWARD}
type
  TUnresolvedClass = class;
{$endif}

{$ifdef TEST_DUPLICATE_CLASS_FORWARD}
type
  TDuplicateClass = class;
  TDuplicateClass = class;
{$endif}

{$ifdef TEST_WRONG_CLASS_FORWARD_COMPLETION}
type
  TWrongClass = class;
  TWrongClass = Integer;
{$endif}

{$ifdef TEST_FORWARD_CLASS_SUPER}
type
  TForwardSuper = class;
  TBadChild = class(TForwardSuper)
  end;
  TForwardSuper = class
  end;
{$endif}

{$ifdef TEST_EXTERNAL_NIL_REJECTED}
procedure OldExternalSpelling;
  external nil name 'p_old_external_spelling';
{$endif}

procedure TBase.SetValue(NewValue: LongInt);
begin
  Value := NewValue
end;

function TBase.GetValue: LongInt;
begin
  GetValue := 10
end;

procedure TChild.UseInheritedMembers;
begin
  SetValue(20);
  Value := Value + 1
end;

function TChild.GetValue: LongInt;
begin
  GetValue := inherited GetValue + 1
end;

begin
  ForwardGlobal := nil
end.
