program SetArithmeticRejected;

type
  TFirst = (FirstA, FirstB);
  TSecond = (SecondA, SecondB);
  TFirstSet = set of TFirst;
  TSecondSet = set of TSecond;

var
  FirstValues: TFirstSet;
  SecondValues: TSecondSet;
  ResultValues: TFirstSet;

begin
  ResultValues := FirstValues + SecondValues
end.
