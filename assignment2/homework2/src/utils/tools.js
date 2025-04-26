function getRotationPrecomputeL(precompute_L, rotationMatrix){

	// Step 1: Calc each band's rotation matrix 
	// Caution: for each dir the Rotation matrix is a inverse of the rotation matrix 
    let r = mat4Matrix2mathMatrix(rotationMatrix);

    let shRotateMatrix3x3 = computeSquareMatrix_3by3(r);
    let shRotateMatrix5x5 = computeSquareMatrix_5by5(r);

	// Step 2: Initialize rotated L    //      R   G   B
    let result = [];                   // SH0
    for(let i = 0; i < 9; i++){        // SH1
        result[i] = [];                // SH2
    }                                  // ...

	// Step 3: Calc SH coeffs for RGB component of precompute_L
    for(let i = 0; i < 3; i++){
        let L_SH_R_3 = math.multiply([precompute_L[1][i], precompute_L[2][i], precompute_L[3][i]], shRotateMatrix3x3);
        let L_SH_R_5 = math.multiply([precompute_L[4][i], precompute_L[5][i], precompute_L[6][i], precompute_L[7][i], precompute_L[8][i]], shRotateMatrix5x5);
		
		let L_SH_R_3_arr = L_SH_R_3.toArray();
		let L_SH_R_5_arr = L_SH_R_5.toArray();

        result[0][i] = precompute_L[0][i]; // Y1^{0} remains the same
		result[1][i] = L_SH_R_3_arr[0];
        result[2][i] = L_SH_R_3_arr[1];
        result[3][i] = L_SH_R_3_arr[2];
        result[4][i] = L_SH_R_5_arr[0];
        result[5][i] = L_SH_R_5_arr[1];
        result[6][i] = L_SH_R_5_arr[2];
        result[7][i] = L_SH_R_5_arr[3];
        result[8][i] = L_SH_R_5_arr[4];
    }

	return result;
}

function computeSquareMatrix_3by3(rotationMatrix){ // 计算方阵SA(-1) 3*3 
	
	// 1、pick ni - {ni}
	let n1 = [1, 0, 0, 0]; let n2 = [0, 0, 1, 0]; let n3 = [0, 1, 0, 0];

	// 2、{P(ni)} - A  A_inverse
	let pSHn1 = SHEval(n1[0], n1[1], n1[2], 3);
	let pSHn2 = SHEval(n2[0], n2[1], n2[2], 3);
	let pSHn3 = SHEval(n3[0], n3[1], n3[2], 3);
	
	let A = math.matrix([
		[pSHn1[1], pSHn2[1], pSHn3[1]], // Y1^{-1} 
		[pSHn1[2], pSHn2[2], pSHn3[2]], // Y1^{0}
		[pSHn1[3], pSHn2[3], pSHn3[3]]  // Y1^{1}
	]);
	let A_inverse = math.inv(A);

	// 3、用 R 旋转 ni - {R(ni)}
	let Rn1 = math.multiply(rotationMatrix, n1);
	let Rn2 = math.multiply(rotationMatrix, n2);
	let Rn3 = math.multiply(rotationMatrix, n3);

	// get array from math matrix
	let Rn1_arr = Rn1.toArray();
	let Rn2_arr = Rn2.toArray();
	let Rn3_arr = Rn3.toArray();

	// 4、R(ni) SH投影 - S
	let pSHRn1 = SHEval(Rn1_arr[0], Rn1_arr[1], Rn1_arr[2], 3);
	let pSHRn2 = SHEval(Rn2_arr[0], Rn2_arr[1], Rn2_arr[2], 3);
	let pSHRn3 = SHEval(Rn3_arr[0], Rn3_arr[1], Rn3_arr[2], 3);

	let S = math.matrix([
		[pSHRn1[1], pSHRn2[1], pSHRn3[1]], // Y1^{-1}
		[pSHRn1[2], pSHRn2[2], pSHRn3[2]], // Y1^{0}
		[pSHRn1[3], pSHRn2[3], pSHRn3[3]]  // Y1^{1}
	]);

	// 5、S*A_inverse
	let SA_inverse = math.multiply(S, A_inverse);
	return SA_inverse;
}

function computeSquareMatrix_5by5(rotationMatrix){ // 计算方阵SA(-1) 5*5
	
	// 1、pick ni - {ni}
	let k = 1 / math.sqrt(2);
	let n1 = [1, 0, 0, 0]; let n2 = [0, 0, 1, 0]; let n3 = [k, k, 0, 0]; 
	let n4 = [k, 0, k, 0]; let n5 = [0, k, k, 0];

	// 2、{P(ni)} - A  A_inverse
	let pSHn1 = SHEval(n1[0], n1[1], n1[2], 3);
	let pSHn2 = SHEval(n2[0], n2[1], n2[2], 3);
	let pSHn3 = SHEval(n3[0], n3[1], n3[2], 3);
	let pSHn4 = SHEval(n4[0], n4[1], n4[2], 3);
	let pSHn5 = SHEval(n5[0], n5[1], n5[2], 3);

	let A = math.matrix([
		[pSHn1[4], pSHn2[4], pSHn3[4], pSHn4[4], pSHn5[4]], // Y2^{-2} 
		[pSHn1[5], pSHn2[5], pSHn3[5], pSHn4[5], pSHn5[5]], // Y2^{-1}
		[pSHn1[6], pSHn2[6], pSHn3[6], pSHn4[6], pSHn5[6]], // Y2^{0}
		[pSHn1[7], pSHn2[7], pSHn3[7], pSHn4[7], pSHn5[7]], // Y2^{1}
		[pSHn1[8], pSHn2[8], pSHn3[8], pSHn4[8], pSHn5[8]]  // Y2^{2}
	]);
	let A_inverse = math.inv(A);

	// 3、用 R 旋转 ni - {R(ni)}
	let Rn1 = math.multiply(rotationMatrix, n1);
	let Rn2 = math.multiply(rotationMatrix, n2);
	let Rn3 = math.multiply(rotationMatrix, n3);
	let Rn4 = math.multiply(rotationMatrix, n4);
	let Rn5 = math.multiply(rotationMatrix, n5);

	// get array from math matrix
	let Rn1_arr = Rn1.toArray();
	let Rn2_arr = Rn2.toArray();
	let Rn3_arr = Rn3.toArray();
	let Rn4_arr = Rn4.toArray();
	let Rn5_arr = Rn5.toArray();

	// 4、R(ni) SH投影 - S
	let pSHRn1 = SHEval(Rn1_arr[0], Rn1_arr[1], Rn1_arr[2], 3);
	let pSHRn2 = SHEval(Rn2_arr[0], Rn2_arr[1], Rn2_arr[2], 3);
	let pSHRn3 = SHEval(Rn3_arr[0], Rn3_arr[1], Rn3_arr[2], 3);
	let pSHRn4 = SHEval(Rn4_arr[0], Rn4_arr[1], Rn4_arr[2], 3);
	let pSHRn5 = SHEval(Rn5_arr[0], Rn5_arr[1], Rn5_arr[2], 3);

	let S = math.matrix([
		[pSHRn1[4], pSHRn2[4], pSHRn3[4], pSHRn4[4], pSHRn5[4]], // Y2^{-2}
		[pSHRn1[5], pSHRn2[5], pSHRn3[5], pSHRn4[5], pSHRn5[5]], // Y2^{-1}
		[pSHRn1[6], pSHRn2[6], pSHRn3[6], pSHRn4[6], pSHRn5[6]], // Y2^{0}
		[pSHRn1[7], pSHRn2[7], pSHRn3[7], pSHRn4[7], pSHRn5[7]], // Y2^{1}
		[pSHRn1[8], pSHRn2[8], pSHRn3[8], pSHRn4[8], pSHRn5[8]]  // Y2^{2}
	]);

	// 5、S*A_inverse
	let SA_inverse = math.multiply(S, A_inverse);
	return SA_inverse;
}

function mat4Matrix2mathMatrix(rotationMatrix){

	let mathMatrix = [];
	for(let i = 0; i < 4; i++){
		let r = [];
		for(let j = 0; j < 4; j++){
			r.push(rotationMatrix[i*4+j]);
		}
		mathMatrix.push(r);
	}
	return math.matrix(mathMatrix)

}

function getMat3ValueFromRGB(precomputeL){

    let colorMat3 = [];
    for(var i = 0; i<3; i++){
        colorMat3[i] = mat3.fromValues( precomputeL[0][i], precomputeL[1][i], precomputeL[2][i],
										precomputeL[3][i], precomputeL[4][i], precomputeL[5][i],
										precomputeL[6][i], precomputeL[7][i], precomputeL[8][i] ); 
	}
    return colorMat3;
}