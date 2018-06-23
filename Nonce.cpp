#include "Nonce.h"
#include "shabal.h"
#include "mshabal.h"
#include "mshabal256.h"



namespace AVX1
{
	void work_i(const size_t local_num, unsigned long long loc_addr, unsigned long long local_startnonce, const unsigned long long local_nonces)
	{
		unsigned long long nonce;
		unsigned long long nonce1;
		unsigned long long nonce2;
		unsigned long long nonce3;
		unsigned long long nonce4;

		char *final = new char[32];
		char *gendata = new char[PLOT_SIZE +16];
		char *final1 = new char[32];
		char *final2 = new char[32];
		char *final3 = new char[32];
		char *final4 = new char[32];
		char *gendata1 = new char[PLOT_SIZE + 16];
		char *gendata2 = new char[PLOT_SIZE + 16];
		char *gendata3 = new char[PLOT_SIZE + 16];
		char *gendata4 = new char[PLOT_SIZE + 16];
		
		size_t len;
		shabal_context *x = new shabal_context[sizeof(shabal_context)];
		mshabal_context *mx = new mshabal_context[sizeof(mshabal_context)];

		int8_t *xv = reinterpret_cast<int8_t*>(&loc_addr); // optimization: reduced 8*4*nonces assignments
		for (size_t i = 0; i < 8; i++)
		{
			gendata[PLOT_SIZE + i] = xv[7 - i];
			gendata1[PLOT_SIZE + i] = xv[7 - i];
			gendata2[PLOT_SIZE + i] = xv[7 - i];
			gendata3[PLOT_SIZE + i] = xv[7 - i];
			gendata4[PLOT_SIZE + i] = xv[7 - i];
		}
		char *xv1, *xv2, *xv3, *xv4;
		
		shabal_context *init_x = new shabal_context[sizeof(shabal_context)];
		shabal_init(init_x, 256);
		mshabal_context *init_mx = new mshabal_context[sizeof(mshabal_context)];
		avx1_mshabal_init(init_mx, 256);

		for (unsigned long long n = 0; n < local_nonces;)
		{
			if (n + 4 <= local_nonces)
			{
				nonce1 = local_startnonce + n + 0;
				nonce2 = local_startnonce + n + 1;
				nonce3 = local_startnonce + n + 2;
				nonce4 = local_startnonce + n + 3;

				xv1 = reinterpret_cast<char*>(&nonce1);
				xv2 = reinterpret_cast<char*>(&nonce2);
				xv3 = reinterpret_cast<char*>(&nonce3);
				xv4 = reinterpret_cast<char*>(&nonce4);
				for (size_t i = 8; i < 16; i++)
				{
					gendata1[PLOT_SIZE + i] = xv1[15 - i];
					gendata2[PLOT_SIZE + i] = xv2[15 - i];
					gendata3[PLOT_SIZE + i] = xv3[15 - i];
					gendata4[PLOT_SIZE + i] = xv4[15 - i];
				}
				
				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: avx1_mshabal_init(mx, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;
					avx1_mshabal(mx, &gendata1[i], &gendata2[i], &gendata3[i], &gendata4[i], len);
					avx1_mshabal_close(mx, 0, 0, 0, 0, 0, &gendata1[i - HASH_SIZE], &gendata2[i - HASH_SIZE], &gendata3[i - HASH_SIZE], &gendata4[i - HASH_SIZE]);
				}

				memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: avx1_mshabal_init(mx, 256);
				avx1_mshabal(mx, gendata1, gendata2, gendata3, gendata4, PLOT_SIZE + 16);
				avx1_mshabal_close(mx, 0, 0, 0, 0, 0, final1, final2, final3, final4);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)
				{
					gendata1[i] ^= (final1[i % 32]);
					gendata2[i] ^= (final2[i % 32]);
					gendata3[i] ^= (final3[i % 32]);
					gendata4[i] ^= (final4[i % 32]);
				}

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i ++)
				{
					memmove(&cache[i][(n + 0 + local_num*local_nonces) * SCOOP_SIZE], &gendata1[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 1 + local_num*local_nonces) * SCOOP_SIZE], &gendata2[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 2 + local_num*local_nonces) * SCOOP_SIZE], &gendata3[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 3 + local_num*local_nonces) * SCOOP_SIZE], &gendata4[i * SCOOP_SIZE], SCOOP_SIZE);
				}
				n += 4;
			}
			else 
			{
				_mm256_zeroupper();
				nonce = local_startnonce + n;
				xv = reinterpret_cast<int8_t*>(&nonce);

				for (size_t i = 8; i < 16; i++)	gendata[PLOT_SIZE + i] = xv[15 - i];
				
				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(x, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;

					shabal(x, &gendata[i], len);
					shabal_close(x, 0, 0, &gendata[i - HASH_SIZE]);
				}

				memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(x, 256);
				shabal(x, gendata, 16 + PLOT_SIZE);
				shabal_close(x, 0, 0, final);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)			gendata[i] ^= (final[i % HASH_SIZE]);

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i++)		memmove(&cache[i][(n + local_num*local_nonces) * SCOOP_SIZE], &gendata[i * SCOOP_SIZE], SCOOP_SIZE); 
				n++;
			}
			worker_status[local_num] = n;
		}
		delete[] final;
		delete[] gendata;
		delete[] gendata1;
		delete[] gendata2;
		delete[] gendata3;
		delete[] gendata4;
		delete[] final1;
		delete[] final2;
		delete[] final3;
		delete[] final4;
		delete[] x;
		delete[] mx;
		delete[] init_mx;
		delete[] init_x;

		return;
	}
}

////////////////////////////////////////////////////////
namespace AVX2
{
	void work_i(const size_t local_num, unsigned long long loc_addr, const unsigned long long local_startnonce, const unsigned long long local_nonces)
	{
		unsigned long long nonce;
		unsigned long long nonce1;
		unsigned long long nonce2;
		unsigned long long nonce3;
		unsigned long long nonce4;
		unsigned long long nonce5;
		unsigned long long nonce6;
		unsigned long long nonce7;
		unsigned long long nonce8;

		char *final = new char[32];
		char *gendata = new char[16 + PLOT_SIZE];
		char *final1 = new char[32];
		char *final2 = new char[32];
		char *final3 = new char[32];
		char *final4 = new char[32];
		char *final5 = new char[32];
		char *final6 = new char[32];
		char *final7 = new char[32];
		char *final8 = new char[32];
		char *gendata1 = new char[16 + PLOT_SIZE];
		char *gendata2 = new char[16 + PLOT_SIZE];
		char *gendata3 = new char[16 + PLOT_SIZE];
		char *gendata4 = new char[16 + PLOT_SIZE];
		char *gendata5 = new char[16 + PLOT_SIZE];
		char *gendata6 = new char[16 + PLOT_SIZE];
		char *gendata7 = new char[16 + PLOT_SIZE];
		char *gendata8 = new char[16 + PLOT_SIZE];

		size_t len;
		shabal_context *x = new shabal_context[sizeof(shabal_context)];
		mshabal256_context *mx = new mshabal256_context[sizeof(mshabal256_context)];

		int8_t *xv = reinterpret_cast<int8_t*>(&loc_addr); // optimization: reduced 8*4*nonces assignments
		for (size_t i = 0; i < 8; i++)
		{
			gendata1[PLOT_SIZE + i] = xv[7 - i];
			gendata2[PLOT_SIZE + i] = xv[7 - i];
			gendata3[PLOT_SIZE + i] = xv[7 - i];
			gendata4[PLOT_SIZE + i] = xv[7 - i];
			gendata5[PLOT_SIZE + i] = xv[7 - i];
			gendata6[PLOT_SIZE + i] = xv[7 - i];
			gendata7[PLOT_SIZE + i] = xv[7 - i];
			gendata8[PLOT_SIZE + i] = xv[7 - i];
		}
		char *xv1, *xv2, *xv3, *xv4, *xv5, *xv6, *xv7, *xv8;

		shabal_context *init_x = new shabal_context[sizeof(shabal_context)];
		shabal_init(init_x, 256);
		mshabal256_context *init_mx = new mshabal256_context[sizeof(mshabal256_context)];
		mshabal256_init(init_mx, 256);

		for (unsigned long long n = 0; n < local_nonces;)
		{
			if (n + 8 <= local_nonces)
			{
				nonce1 = local_startnonce + n + 0;
				nonce2 = local_startnonce + n + 1;
				nonce3 = local_startnonce + n + 2;
				nonce4 = local_startnonce + n + 3;
				nonce5 = local_startnonce + n + 4;
				nonce6 = local_startnonce + n + 5;
				nonce7 = local_startnonce + n + 6;
				nonce8 = local_startnonce + n + 7;
				xv1 = reinterpret_cast<char*>(&nonce1);
				xv2 = reinterpret_cast<char*>(&nonce2);
				xv3 = reinterpret_cast<char*>(&nonce3);
				xv4 = reinterpret_cast<char*>(&nonce4);
				xv5 = reinterpret_cast<char*>(&nonce5);
				xv6 = reinterpret_cast<char*>(&nonce6);
				xv7 = reinterpret_cast<char*>(&nonce7);
				xv8 = reinterpret_cast<char*>(&nonce8);
				for (size_t i = 8; i < 16; i++)
				{
					gendata1[PLOT_SIZE + i] = xv1[15 - i];
					gendata2[PLOT_SIZE + i] = xv2[15 - i];
					gendata3[PLOT_SIZE + i] = xv3[15 - i];
					gendata4[PLOT_SIZE + i] = xv4[15 - i];
					gendata5[PLOT_SIZE + i] = xv5[15 - i];
					gendata6[PLOT_SIZE + i] = xv6[15 - i];
					gendata7[PLOT_SIZE + i] = xv7[15 - i];
					gendata8[PLOT_SIZE + i] = xv8[15 - i];
				}

				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: mshabal256_init(mx, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;
					mshabal256(mx, &gendata1[i], &gendata2[i], &gendata3[i], &gendata4[i], &gendata5[i], &gendata6[i], &gendata7[i], &gendata8[i], len);
					mshabal256_close(mx, 0, 0, 0, 0, 0, 0, 0, 0, 0, &gendata1[i - HASH_SIZE], &gendata2[i - HASH_SIZE], &gendata3[i - HASH_SIZE], &gendata4[i - HASH_SIZE], &gendata5[i - HASH_SIZE], &gendata6[i - HASH_SIZE], &gendata7[i - HASH_SIZE], &gendata8[i - HASH_SIZE]);
				}

				memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: mshabal256_init(mx, 256);
				mshabal256(mx, gendata1, gendata2, gendata3, gendata4, gendata5, gendata6, gendata7, gendata8, 16 + PLOT_SIZE);
				mshabal256_close(mx, 0, 0, 0, 0, 0, 0, 0, 0, 0, final1, final2, final3, final4, final5, final6, final7, final8);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)
				{
					gendata1[i] ^= (final1[i % 32]);
					gendata2[i] ^= (final2[i % 32]);
					gendata3[i] ^= (final3[i % 32]);
					gendata4[i] ^= (final4[i % 32]);
					gendata5[i] ^= (final5[i % 32]);
					gendata6[i] ^= (final6[i % 32]);
					gendata7[i] ^= (final7[i % 32]);
					gendata8[i] ^= (final8[i % 32]);
				}

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i++)
				{
					memmove(&cache[i][(n + 0 + local_num*local_nonces) * SCOOP_SIZE], &gendata1[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 1 + local_num*local_nonces) * SCOOP_SIZE], &gendata2[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 2 + local_num*local_nonces) * SCOOP_SIZE], &gendata3[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 3 + local_num*local_nonces) * SCOOP_SIZE], &gendata4[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 4 + local_num*local_nonces) * SCOOP_SIZE], &gendata5[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 5 + local_num*local_nonces) * SCOOP_SIZE], &gendata6[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 6 + local_num*local_nonces) * SCOOP_SIZE], &gendata7[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 7 + local_num*local_nonces) * SCOOP_SIZE], &gendata8[i * SCOOP_SIZE], SCOOP_SIZE);
				}
				n += 8;
			}
			else
			{
				_mm256_zeroupper();
				nonce = local_startnonce + n;
				xv = reinterpret_cast<int8_t*>(&nonce);

				for (size_t i = 8; i < 16; i++)	gendata[PLOT_SIZE + i] = xv[15 - i];

				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(mx, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;

					shabal(x, &gendata[i], len);
					shabal_close(x, 0, 0, &gendata[i - HASH_SIZE]);
				}

				memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(mx, 256);
				shabal(x, gendata, 16 + PLOT_SIZE);
				shabal_close(x, 0, 0, final);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)			gendata[i] ^= (final[i % HASH_SIZE]);

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i++)		memmove(&cache[i][(n + local_num*local_nonces) * SCOOP_SIZE], &gendata[i * SCOOP_SIZE], SCOOP_SIZE);
				n++;
			}
			worker_status[local_num] = n;
		}
		delete[] final;
		delete[] gendata;
		delete[] gendata1;
		delete[] gendata2;
		delete[] gendata3;
		delete[] gendata4;
		delete[] gendata5;
		delete[] gendata6;
		delete[] gendata7;
		delete[] gendata8;
		delete[] final1;
		delete[] final2;
		delete[] final3;
		delete[] final4;
		delete[] final5;
		delete[] final6;
		delete[] final7;
		delete[] final8;
		delete[] x;
		delete[] mx;
		delete[] init_mx;
		delete[] init_x;

		return;
	}
}
///////////////////////////////////////////////


namespace SSE4
{
	void work_i(const size_t local_num, unsigned long long loc_addr, const unsigned long long local_startnonce, const unsigned long long local_nonces)
	{
		unsigned long long nonce;
		unsigned long long nonce1;
		unsigned long long nonce2;
		unsigned long long nonce3;
		unsigned long long nonce4;

		char *final = new char[32];
		char *gendata = new char[16 + PLOT_SIZE];
		char *final1 = new char[32];
		char *final2 = new char[32];
		char *final3 = new char[32];
		char *final4 = new char[32];
		char *gendata1 = new char[16 + PLOT_SIZE];
		char *gendata2 = new char[16 + PLOT_SIZE];
		char *gendata3 = new char[16 + PLOT_SIZE];
		char *gendata4 = new char[16 + PLOT_SIZE];

		size_t len;
		shabal_context *x = new shabal_context[sizeof(shabal_context)];
		mshabal_context *mx = new mshabal_context[sizeof(mshabal_context)];

		int8_t *xv = reinterpret_cast<int8_t*>(&loc_addr); // optimization: reduced 8*4*nonces assignments
		for (size_t i = 0; i < 8; i++)
		{
			gendata[PLOT_SIZE + i] = xv[7 - i];
			gendata1[PLOT_SIZE + i] = xv[7 - i];
			gendata2[PLOT_SIZE + i] = xv[7 - i];
			gendata3[PLOT_SIZE + i] = xv[7 - i];
			gendata4[PLOT_SIZE + i] = xv[7 - i];
		}
		char *xv1, *xv2, *xv3, *xv4;

		shabal_context *init_x = new shabal_context[sizeof(shabal_context)];
		shabal_init(init_x, 256);
		mshabal_context *init_mx = new mshabal_context[sizeof(mshabal_context)];
		sse4_mshabal_init(init_mx, 256);

		for (unsigned long long n = 0; n < local_nonces;)
		{
			if (n + 4 <= local_nonces)
			{
				nonce1 = local_startnonce + n + 0;
				nonce2 = local_startnonce + n + 1;
				nonce3 = local_startnonce + n + 2;
				nonce4 = local_startnonce + n + 3;
				xv1 = reinterpret_cast<char*>(&nonce1);
				xv2 = reinterpret_cast<char*>(&nonce2);
				xv3 = reinterpret_cast<char*>(&nonce3);
				xv4 = reinterpret_cast<char*>(&nonce4);
				
				for (size_t i = 8; i < 16; i++)
				{
					gendata1[PLOT_SIZE + i] = xv1[15 - i];
					gendata2[PLOT_SIZE + i] = xv2[15 - i];
					gendata3[PLOT_SIZE + i] = xv3[15 - i];
					gendata4[PLOT_SIZE + i] = xv4[15 - i];
				}

				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: sse4_mshabal_init(mx, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;
					sse4_mshabal(mx, &gendata1[i], &gendata2[i], &gendata3[i], &gendata4[i], len);
					sse4_mshabal_close(mx, 0, 0, 0, 0, 0, &gendata1[i - HASH_SIZE], &gendata2[i - HASH_SIZE], &gendata3[i - HASH_SIZE], &gendata4[i - HASH_SIZE]);
				}

				memcpy(mx, init_mx, sizeof(*init_mx)); // optimization: sse4_mshabal_init(mx, 256);
				sse4_mshabal(mx, gendata1, gendata2, gendata3, gendata4, 16 + PLOT_SIZE);
				sse4_mshabal_close(mx, 0, 0, 0, 0, 0, final1, final2, final3, final4);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)
				{
					gendata1[i] ^= (final1[i % 32]);
					gendata2[i] ^= (final2[i % 32]);
					gendata3[i] ^= (final3[i % 32]);
					gendata4[i] ^= (final4[i % 32]);
				}

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i++)
				{
					memmove(&cache[i][(n + 0 + local_num*local_nonces) * SCOOP_SIZE], &gendata1[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 1 + local_num*local_nonces) * SCOOP_SIZE], &gendata2[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 2 + local_num*local_nonces) * SCOOP_SIZE], &gendata3[i * SCOOP_SIZE], SCOOP_SIZE);
					memmove(&cache[i][(n + 3 + local_num*local_nonces) * SCOOP_SIZE], &gendata4[i * SCOOP_SIZE], SCOOP_SIZE);
				}
				n += 4;
			}
			else
			{
				nonce = local_startnonce + n;
				xv = reinterpret_cast<int8_t*>(&nonce);

				for (size_t i = 8; i < 16; i++)	gendata[PLOT_SIZE + i] = xv[15 - i];

				for (size_t i = PLOT_SIZE; i > 0; i -= HASH_SIZE)
				{
					memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(x, 256);
					if (i < PLOT_SIZE + 16 - HASH_CAP) len = HASH_CAP;  // optimization: reduced 8064 assignments
					else len = PLOT_SIZE + 16 - i;

					shabal(x, &gendata[i], len);
					shabal_close(x, 0, 0, &gendata[i - HASH_SIZE]);
				}

				//shabal_init(x, 256);
				memcpy(x, init_x, sizeof(*init_x)); // optimization: shabal_init(x, 256);
				shabal(x, gendata, 16 + PLOT_SIZE);
				shabal_close(x, 0, 0, final);

				// XOR with final
				for (size_t i = 0; i < PLOT_SIZE; i++)			gendata[i] ^= (final[i % HASH_SIZE]);

				// Sort them:
				for (size_t i = 0; i < HASH_CAP; i++)		memmove(&cache[i][(n + local_num*local_nonces) * SCOOP_SIZE], &gendata[i * SCOOP_SIZE], SCOOP_SIZE);
				n++;
			}
			worker_status[local_num] = n;
		}
		delete[] final;
		delete[] gendata;
		delete[] gendata1;
		delete[] gendata2;
		delete[] gendata3;
		delete[] gendata4;
		delete[] final1;
		delete[] final2;
		delete[] final3;
		delete[] final4;
		delete[] x;
		delete[] mx;
		delete[] init_mx;
		delete[] init_x;

		return;
	}
}

