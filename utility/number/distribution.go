package number

// TODO We should have some distribution tests

type Distribution interface {
   New(uint64) Distribution
   LessThan(uint64) uint64
}