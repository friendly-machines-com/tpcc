program ClassReferenceConstant;

type
  TFoo = class(TObject)
  end;
  TFooClass = class of TFoo;

const
  CFoo: TFooClass = TFoo;
  CObj: TClass = TFoo;

var
  K: TFooClass;

begin
  K := CFoo;
  WriteLn(K.ClassName);
  WriteLn(CObj.ClassName);
  if K.InstanceSize <> TFoo.InstanceSize then
    Halt(1);
  if CObj.InheritsFrom(TObject) then
    WriteLn('inherits')
end.
