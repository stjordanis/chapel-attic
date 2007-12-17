// Copy matrix B's transpose into matrix A
def transposeCopy(A: [] real, B: [] real) where A.rank == 2 && B.rank == 2 {
    forall (i, j) in A.domain do
        A[i, j] = B[j, i];
}

// Vector-Matrix transformation
def *(V: [] real, M: [] real) where V.rank == 1 && M.rank == 2 {
    var R: [M.domain.dim(2)] real = 0.0;

    if V.domain.dim(1) != M.domain.dim(1) then
        halt("*: Vector and matrix dims must match");

    [i in R.domain] R[i] = + reduce(V * M(..,i));

    return R;
}

// Matrix-Vector multiplication 
def *(M: [] real, V: [] real) where V.rank == 1 && M.rank == 2 {
    var R: [M.domain.dim(1)] real = 0.0;

    if V.domain.dim(1) != M.domain.dim(2) then
        halt("*: Vector and matrix dims must match");

    [i in R.domain] R[i] = + reduce(M(i,..) * V);

    return R;
}

// Frobenius Norm -- 2-norm for Vectors
def normf(A) where A.rank == 1 {
    return sqrt(+ reduce A**2);
}

// Inner product of A and B
def inner(A, B) where A.rank == 1 && B.rank == 1 {
    return + reduce A*B;
}
