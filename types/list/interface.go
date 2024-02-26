package list

type Size = uint64

type Position = uint64

type Data = uint64

type Structure[T any] interface {

   Verify()

   New() T

   Clone() T

   Free()

   Size() Size

   Each(func(Data))
}

// https://en.wikipedia.org/wiki/List_(abstract_data_type)
type List interface {
   Structure[List]

   Select(Position) Data

   Update(Position, Data)

   Insert(Position, Data)

   Delete(Position) Data

   Split(Position) (List, List)

   Join(List) List
}
