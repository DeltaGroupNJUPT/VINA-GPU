void quaternion_increment(float* q, const float* rotation, float epsilon_fl);
void normalize_angle(float* x);
inline void output_type_cl_init(output_type_cl* out, __constant float* ptr) {
	for (int i = 0; i < 3; i++)out->position[i] = ptr[i];
	for (int i = 0; i < 4; i++)out->orientation[i] = ptr[i + 3];
	for (int i = 0; i < MAX_NUM_OF_LIG_TORSION; i++)out->lig_torsion[i] = ptr[i + 3 + 4];
	for (int i = 0; i < MAX_NUM_OF_FLEX_TORSION; i++)out->flex_torsion[i] = ptr[i + 3 + 4 + MAX_NUM_OF_LIG_TORSION];
	out->lig_torsion_size = ptr[3 + 4 + MAX_NUM_OF_LIG_TORSION + MAX_NUM_OF_FLEX_TORSION];
	//没有赋值coords和e
}

inline void output_type_cl_init_with_output(output_type_cl* out_new, const output_type_cl* out_old) {
	for (int i = 0; i < 3; i++)out_new->position[i] = out_old->position[i];
	for (int i = 0; i < 4; i++)out_new->orientation[i] = out_old->orientation[i];
	for (int i = 0; i < MAX_NUM_OF_LIG_TORSION; i++)out_new->lig_torsion[i] = out_old->lig_torsion[i];
	for (int i = 0; i < MAX_NUM_OF_FLEX_TORSION; i++)out_new->flex_torsion[i] = out_old->flex_torsion[i];
	out_new->lig_torsion_size = out_old->lig_torsion_size;
	//赋值了e,没有赋值coords
	out_new->e = out_old->e;
	//for (int i = 0; i < MAX_NUM_OF_ATOMS; i++) {
	//	for (int j = 0; j < 3; j++) {
	//		out_new->coords[i][j] = out_old->coords[i][j];
	//	}
	//}
}

void output_type_cl_increment(output_type_cl* x, const change_cl* c, float factor, float epsilon_fl) {
	// position increment
	for (int k = 0; k < 3; k++) x->position[k] += factor * c->position[k];
	// orientation increment
	float rotation[3];
	for (int k = 0; k < 3; k++) rotation[k] = factor * c->orientation[k];
	//for (int k = 0; k < 4; k++)printf("\norientation[%d] = %f", k,x->orientation[k]);
	quaternion_increment(x->orientation, rotation, epsilon_fl);
	
	// torsion increment
	for (int k = 0; k < x->lig_torsion_size; k++) {
		float tmp = factor * c->lig_torsion[k];
		normalize_angle(&tmp);
		x->lig_torsion[k] += tmp;
		normalize_angle(&(x->lig_torsion[k]));
	}
}

inline float norm3(const float* a) {
	return sqrt(pown(a[0], 2) + pown(a[1], 2) + pown(a[2], 2));
}

inline void normalize_angle(float* x) {
	while (1) {
		if (*x >= -(M_PI) && *x <= (M_PI)) {
			break;
		}
		else if (*x > 3 * (M_PI)) {
			float n = (*x - (M_PI)) / (2 * (M_PI));
			*x -= 2 * (M_PI) * ceil(n);
		}
		else if (*x < 3 * -(M_PI)) {
			float n = (-*x - (M_PI)) / (2 * (M_PI));
			*x += 2 * (M_PI) * ceil(n);
		}
		else if (*x > (M_PI)) {
			*x -= 2 * (M_PI);
		}
		else if (*x < -(M_PI)) {
			*x += 2 * (M_PI);
		}
		else {
			printf("\n kernel2: normalize_angle() ERROR! x = %f M_PI = %f", *x,M_PI);
			break;
		}
	}
}

inline bool quaternion_is_normalized(float* q) {
	float q_pow = pown(q[0], 2) + pown(q[1], 2) + pown(q[2], 2) + pown(q[3], 2);
	float sqrt_q_pow = sqrt(q_pow);
	return (q_pow - 1 < 0.001) && (sqrt_q_pow - 1 < 0.001);
}

inline void angle_to_quaternion(float* q, const float* rotation, float epsilon_fl) {
	float angle = norm3(rotation);
	//printf("\nangle = %f", angle);
	if (angle > epsilon_fl) {
		float axis[3] = { rotation[0] / angle, rotation[1] / angle ,rotation[2] / angle };
		if (norm3(axis) - 1 >= 0.001)printf("kernel2: angle_to_quaternion() ERROR!"); // Replace assert(eq(axis.norm(), 1));
		normalize_angle(&angle);
		//printf("\nangle = %f", angle);
		float c = cos(angle / 2);
		float s = sin(angle / 2);
		//printf("\n c = %f s = %f", c, s);
		//printf("\n axis[0] = %f, axis[1] = %f,axis[2] = %f", axis[0], axis[1], axis[2]);
		q[0] = c; q[1] = s * axis[0]; q[2] = s * axis[1]; q[3] = s * axis[2];
		return;
	}
	q[0] = 1; q[1] = 0; q[2] = 0; q[3] = 0;
	return;
}

// quaternion multiplication
inline void angle_to_quaternion_multi(float* qa, const float* qb) {
	float tmp[4] = { qa[0],qa[1],qa[2],qa[3] };
	qa[0] = tmp[0] * qb[0] - tmp[1] * qb[1] - tmp[2] * qb[2] - tmp[3] * qb[3];
	qa[1] = tmp[0] * qb[1] + tmp[1] * qb[0] + tmp[2] * qb[3] - tmp[3] * qb[2];
	qa[2] = tmp[0] * qb[2] - tmp[1] * qb[3] + tmp[2] * qb[0] + tmp[3] * qb[1];
	qa[3] = tmp[0] * qb[3] + tmp[1] * qb[2] - tmp[2] * qb[1] + tmp[3] * qb[0];
}

inline void quaternion_normalize_approx(float* q, float epsilon_fl) {
	const float s = pown(q[0], 2) + pown(q[1], 2) + pown(q[2], 2) + pown(q[3], 2);
	// Omit one assert()
	if (fabs(s - 1) < TOLERANCE)
		;
	else {
		const float a = sqrt(s);
		if (a <= epsilon_fl) printf("kernel2: quaternion_normalize_approx ERROR!"); // Replace assert(a > epsilon_fl);
		for (int i = 0; i < 4; i++)q[i] *= (1 / a);
		if (quaternion_is_normalized(q) != true)printf("kernel2: quaternion_normalize_approx() ERROR!");// Replace assert(quaternion_is_normalized(q));
	}
}

void quaternion_increment(float* q, const float* rotation, float epsilon_fl) {
	if (quaternion_is_normalized(q) != true)printf("kernel2: quaternion_increment() ERROR!"); // Replace assert(quaternion_is_normalized(q))
	float q_old[4] = { q[0],q[1],q[2],q[3] };
	//for (int i = 0; i < 4; i++)printf("\nq[%d] = %f", i, q[i]);
	//for (int i = 0; i < 3; i++)printf("\nrotation[%d] = %f", i, rotation[i]);
	angle_to_quaternion(q, rotation, epsilon_fl);
	//for (int i = 0; i < 4; i++)printf("\nq[%d] = %f", i, q[i]);
	//for (int i = 0; i < 4; i++)printf("\nq_old[%d] = %f", i, q_old[i]);
	angle_to_quaternion_multi(q, q_old);
	quaternion_normalize_approx(q, epsilon_fl);
}


inline float vec_distance_sqr(float* a, float* b) {
	return pown(a[0] - b[0], 2) + pown(a[1] - b[1], 2) + pown(a[2] - b[2], 2);
}

float gyration_radius(						int			m_lig_begin,
											int			m_lig_end,
						//const __global int*			m_atoms_types_gpu,
						const				atom_cl*	atoms,
						const				m_coords_cl*	m_coords_gpu,
						const				float*		m_lig_node_origin
) {
	float acc = 0;
	int counter = 0;
	float origin[3] = { m_lig_node_origin[0], m_lig_node_origin[1], m_lig_node_origin[2] };
	for (int i = m_lig_begin; i < m_lig_end; i++) {
		float current_coords[3] = { m_coords_gpu->coords[i][0], m_coords_gpu->coords[i][1], m_coords_gpu->coords[i][2] };
		if (atoms[i].types[0] != EL_TYPE_H) { // for el, we use the first element (atoms[i].types[0])
			acc += vec_distance_sqr(current_coords, origin);
			++counter;
		}
	}
	return (counter > 0) ? sqrt(acc / counter) : 0;
}

void mutate_conf_cl(const					int				step,
					//const					int				exhaus_id,
					const					int				num_steps,
											output_type_cl*	c,
							__constant		int*			random_int_map_gpu,
							__constant		float			random_inside_sphere_map_gpu[][3],
							__constant		float*			random_fl_pi_map_gpu,
					const					int				m_lig_begin,
					const					int				m_lig_end,
					//const	__global		int*			m_atoms_types_gpu,
					const					atom_cl*		atoms,
					const					m_coords_cl*	m_coords_gpu,
					const					float*			m_lig_node_origin_gpu,
					const					float			epsilon_fl,
					const					float			amplitude
											//int				lig_torsion_size,
											//int				flex_torsion_size,
) {
	//printf("\n out.position[0]=%f",out->position[0]);
	//int mutable_entities_num = lig_torsion_size + flex_torsion_size;
	//if (mutable_entities_num == 0)return;
	int index = step; // global index (among all exhaus)
	int which = random_int_map_gpu[index];
	//which = 1;
	//printf("\n which = %d", which);
	//printf("\n random_inside_sphere_map_gpu = %f %f %f", random_inside_sphere_map_gpu[3 * index], random_inside_sphere_map_gpu[3 * index + 1], random_inside_sphere_map_gpu[3 * index + 2]);
	int lig_torsion_size = c->lig_torsion_size;
	int flex_torsion_size = 0; // FIX? 20210727
	//if (lig_torsion_size != 0) {
		if (which == 0) {
			for (int i = 0; i < 3; i++)
				c->position[i] += amplitude * random_inside_sphere_map_gpu[index][i];
			return;
		}
		--which;
		if (which == 0) {
			float gr = gyration_radius(m_lig_begin, m_lig_end, atoms /*m_atoms_types_gpu*/, m_coords_gpu, m_lig_node_origin_gpu);
			//printf("\n gr = %f", gr);
			if (gr > epsilon_fl) {
				float rotation[3];
				for (int i = 0; i < 3; i++)rotation[i] = amplitude / gr * random_inside_sphere_map_gpu[index][i];
				quaternion_increment(c->orientation, rotation, epsilon_fl);
			}
			return;
		}
		--which;
		if (which < lig_torsion_size) { c->lig_torsion[which] = random_fl_pi_map_gpu[index]; return; }
		which -= lig_torsion_size;
	//}
	//else printf("ERROR: No ligand torsion found!");

	if (flex_torsion_size != 0) {
		if (which < flex_torsion_size) { c->flex_torsion[which] = random_fl_pi_map_gpu[index]; return; }
		which -= flex_torsion_size;
	}
}