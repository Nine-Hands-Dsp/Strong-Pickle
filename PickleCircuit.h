#pragma once
//==============================================================================
// STRONG PICKLE - modele du circuit au niveau des composants (famille Big Muff / Swollen Pickle).
//
// Chaque etage a transistor est resolu a chaque echantillon (methode de Newton sur les tensions des noeuds,
// condensateurs discretises par la methode des trapezes) :
//   - transistor NPN 2N3904 (modele d'Ebers-Moll : saturation, blocage, gain fini),
//   - resistance de contre-reaction collecteur -> base (polarisation) et condensateur Miller 470 pF,
//   - etages d'ecretage : diodes 1N914 tete-beche en serie avec un condensateur de 1 uF (collecteur -> base) ;
//     CRUNCH ajoute une resistance en serie avec les diodes,
//   - liaison d'entree : resistance + condensateur en serie (le grave est retire avant l'ecretage).
// Le son (gain, grain, compression, crachotements de fin de note) vient du circuit lui-meme.
//==============================================================================
#include <cmath>
#include <algorithm>

namespace pickle
{
    struct StageValues
    {
        double rin, cin;        // liaison d'entree (serie)
        double rf, cm;          // contre-reaction collecteur -> base, condensateur Miller
        double rc, re;          // resistance de collecteur, d'emetteur
        bool   diodes;          // etage d'ecretage
        double cd;              // condensateur en serie avec les diodes
        double vcc;
    };

    class TransistorStage
    {
    public:
        using real = double;
        // transistor 2N3904 (Ebers-Moll), diodes 1N914
        real isT = 6.7e-15, bf = 150.0, br = 3.0, vt = 0.02585;
        real isD = 2.52e-9, nD = 1.75;
        real rs = 1.0;                       // resistance serie des diodes (CRUNCH)
        int lastIters = 0;

        void prepare (const StageValues& v, double sampleRate)
        {
            val = v; T = 1.0 / sampleRate;
            // point de repos : quelques secondes de simulation sans signal (les condensateurs se chargent)
            vb = 0.65; vc = v.vcc * 0.5; ve = 0.0; vd = vc;
            capIn = -vb; iIn = 0; capM = vc - vb; iM = 0; capD = 0; iD = 0;
            pB = vb; pC = vc; pE = ve; pD = vd;
            // le condensateur des diodes est maintenu a la tension collecteur - base (diodes a 0 V, comme apres
            // quelques minutes sous tension), puis le circuit tourne librement
            const int nSettle = (int) (0.6 * sampleRate);
            for (int i = 0; i < nSettle; ++i)
            {
                process (0.0f);
                if (i < nSettle * 3 / 4) { capD = vc - vb; iD = 0; vd = vb; pD = vb; }
            }
            pB = vb; pC = vc; pE = ve; pD = vd;
        }

        // un echantillon : u = tension de la source (avant le condensateur de liaison) ; renvoie la tension du collecteur
        inline real process (real u) noexcept
        {
            // compagnons des condensateurs (trapezes)
            // liaison d'entree R-C serie : i = gIn * (u - vb) + hIn
            const real kIn = T / (2.0 * val.cin);
            const real gIn = 1.0 / (val.rin + kIn);
            const real hIn = -(capIn + kIn * iIn) * gIn;
            // Miller C -> B : i = gM * (vc - vb) - hM
            const real gM = 2.0 * val.cm / T;
            const real hM = gM * capM + iM;
            // branche diodes : C -> [rs + Cd] -> d
            const real kD = T / (2.0 * val.cd);
            const real gD = 1.0 / (rs + kD);
            const real hD = -(capD + kD * iD) * gD;
            const real gF = 1.0 / val.rf, gC = 1.0 / val.rc, gE = 1.0 / val.re;
            const real nVtD = nD * vt;
            const real invVt = (real) 1 / vt;

            real B = 2 * vb - pB, C = 2 * vc - pC, E = 2 * ve - pE, D = 2 * vd - pD;   // depart : extrapolation
            for (int it = 0; it < 10; ++it)
            {
                const real vbe = limit (B - E, vb - ve), vbc = limit (B - C, vb - vc);
                // exponentielles en simple precision (largement suffisant) ; jonction base-collecteur bloquee : terme nul
                const real ebe = (real) std::exp ((float) (vbe * invVt));
                const real ebc = vbc < (real) -0.3 ? (real) 0 : (real) std::exp ((float) (vbc * invVt));
                const real ic  = isT * (ebe - ebc) - isT / br * (ebc - 1.0);
                const real ib  = isT / bf * (ebe - 1.0) + isT / br * (ebc - 1.0);
                const real dIcBe = isT / vt * ebe, dIcBc = -isT / vt * ebc - isT / (br * vt) * ebc;
                const real dIbBe = isT / (bf * vt) * ebe, dIbBc = isT / (br * vt) * ebc;

                real idd = 0, gdd = 0;                // diodes tete-beche d -> B
                if (val.diodes)
                {
                    const real vdd = std::max ((real) -1.2, std::min ((real) 1.2, D - B));
                    const real ep = (real) std::exp ((float) (vdd / nVtD)), en = (real) 1 / ep;
                    idd = isD * (ep - en);
                    gdd = isD / nVtD * (ep + en) + (real) 1e-12;
                }

                // residus (somme des courants qui entrent dans chaque noeud)
                const real iSrc = gIn * (u - B) + hIn;
                const real iF   = gF * (C - B);
                const real iMil = gM * (C - B) - hM;
                const real iRC  = gC * (val.vcc - C);
                const real iRD  = val.diodes ? gD * (C - D) + hD : 0.0;
                real f[4];
                f[0] = iSrc + iF + iMil + idd - ib;               // base
                f[1] = iRC - ic - iF - iMil - iRD;                // collecteur
                f[2] = ic + ib - gE * E;                          // emetteur
                f[3] = val.diodes ? iRD - idd : 0.0;              // noeud des diodes

                // jacobien (derivees par rapport a B, C, E, D) ; vbe = B - E, vbc = B - C
                real J[4][4] = {};
                J[0][0] = -gIn - gF - gM - gdd - (dIbBe + dIbBc);
                J[0][1] =  gF + gM + dIbBc;
                J[0][2] =  dIbBe;
                J[0][3] =  gdd;
                J[1][0] = -(dIcBe + dIcBc);
                J[1][1] = -gC + dIcBc - gF - gM - (val.diodes ? gD : 0.0);
                J[1][2] =  dIcBe;
                J[1][3] =  val.diodes ? gD : 0.0;
                J[2][0] =  dIcBe + dIcBc + dIbBe + dIbBc;
                J[2][1] = -(dIcBc + dIbBc);
                J[2][2] = -(dIcBe + dIbBe) - gE;
                J[2][3] =  0.0;
                if (val.diodes)
                {
                    J[3][0] =  gdd;
                    J[3][1] =  gD;
                    J[3][2] =  0.0;
                    J[3][3] = -gD - gdd;
                }
                else { J[3][3] = 1.0; f[3] = 0.0; }

                real dx[4];
                solve4 (J, f, dx);
                // pas limite et tensions gardees dans le domaine physique (alimentation 0 - 9 V) : convergence robuste
                const real stepMax = (real) 0.5;
                for (auto& d : dx) d = std::max (-stepMax, std::min (stepMax, d));
                B -= dx[0]; C -= dx[1]; E -= dx[2]; D -= dx[3];
                const real vmax = (real) (val.vcc + 0.5);
                B = std::max ((real) -3, std::min (vmax, B)); C = std::max ((real) -0.5, std::min (vmax, C));
                E = std::max ((real) -0.5, std::min (vmax, E)); D = std::max ((real) -12, std::min ((real) 20, D));
                const real err = std::abs (dx[0]) + std::abs (dx[1]) + std::abs (dx[2]) + std::abs (dx[3]);
                if (err < (real) 1e-6) { lastIters = it + 1; break; }
                lastIters = it + 1;
            }
            if (! val.diodes) D = C;
            if (! std::isfinite (B + C + E + D) || std::abs (C) > 20) { B = vb; C = vc; E = ve; D = vd; }   // securite

            // mise a jour des etats des condensateurs
            const real iInN = gIn * (u - B) + hIn;
            capIn = capIn + kIn * (iInN + iIn); iIn = iInN;
            const real iMN = gM * (C - B) - hM;
            capM = C - B; iM = iMN;
            if (val.diodes)
            {
                const real iDN = gD * (C - D) + hD;
                capD = capD + kD * (iDN + iD); iD = iDN;
            }
            pB = vb; pC = vc; pE = ve; pD = vd;
            vb = B; vc = C; ve = E; vd = D;
            return C;
        }

        real collector() const { return vc; }

    private:
        // limitation du pas de Newton sur les jonctions (convergence avec l'exponentielle)
        inline real limit (real vnew, real vold) const noexcept
        {
            const real vcrit = (real) 0.75;
            if (vnew > vcrit && std::abs (vnew - vold) > 2.0 * vt)
            {
                const real arg = 1.0 + (vnew - vold) / vt;
                vnew = arg > 0 ? vold + vt * std::log (arg) : vcrit;
            }
            return std::min (vnew, (real) 0.95);
        }
        // elimination de Gauss sans pivotage (la diagonale domine : conductances et jonctions passantes), deroulee
        static inline void solve4 (real A[4][4], const real* b, real* x) noexcept
        {
            real a00 = A[0][0], a01 = A[0][1], a02 = A[0][2], a03 = A[0][3], b0 = b[0];
            real a10 = A[1][0], a11 = A[1][1], a12 = A[1][2], a13 = A[1][3], b1 = b[1];
            real a20 = A[2][0], a21 = A[2][1], a22 = A[2][2], a23 = A[2][3], b2 = b[2];
            real a30 = A[3][0], a31 = A[3][1], a32 = A[3][2], a33 = A[3][3], b3 = b[3];
            real f;
            f = a10 / a00; a11 -= f * a01; a12 -= f * a02; a13 -= f * a03; b1 -= f * b0;
            f = a20 / a00; a21 -= f * a01; a22 -= f * a02; a23 -= f * a03; b2 -= f * b0;
            f = a30 / a00; a31 -= f * a01; a32 -= f * a02; a33 -= f * a03; b3 -= f * b0;
            f = a21 / a11; a22 -= f * a12; a23 -= f * a13; b2 -= f * b1;
            f = a31 / a11; a32 -= f * a12; a33 -= f * a13; b3 -= f * b1;
            f = a32 / a22; a33 -= f * a23; b3 -= f * b2;
            x[3] = b3 / a33;
            x[2] = (b2 - a23 * x[3]) / a22;
            x[1] = (b1 - a12 * x[2] - a13 * x[3]) / a11;
            x[0] = (b0 - a01 * x[1] - a02 * x[2] - a03 * x[3]) / a00;
        }

        StageValues val {};
        real T = 1.0 / 192000.0;
        real vb = 0, vc = 0, ve = 0, vd = 0, pB = 0, pC = 0, pE = 0, pD = 0;
        real capIn = 0, iIn = 0, capM = 0, iM = 0, capD = 0, iD = 0;
    };

    //--------------------------------------------------------------------------
    // Tone stack passif (FILTER) : branche passe-bas (R puis C a la masse) et branche passe-haut (C puis R a la masse),
    // reliees par le potentiometre FILTER ; sortie au curseur. Resolution nodale lineaire (trapezes).
    class ToneStack
    {
    public:
        double rLp = 39e3, cLp = 10e-9, cHp = 4e-9, rHp = 22e3, pot = 100e3;

        void prepare (double sampleRate) { T = 1.0 / sampleRate; a = b = 0; capL = capH = 0; iL = iH = 0; }
        void setPosition (double t) { pos = std::min (0.999, std::max (0.001, t)); }

        // u : tension d'entree (sortie de l'etage precedent, sans sa composante continue)
        inline double process (double u) noexcept
        {
            // noeud A (passe-bas) : courant de R (u -> A), condensateur A -> masse, pot A -> B
            // noeud B (passe-haut) : branche C serie R... modelisee : u -> [C serie] -> B, R de B a la masse, pot B -> A
            const double gR = 1.0 / rLp, gCL = 2.0 * cLp / T, gRh = 1.0 / rHp;
            const double gCH = 2.0 * cHp / T;
            const double gP = 1.0 / pot;                        // le pot complet relie A et B
            // compagnons : condensateur A->masse : i = gCL * A - (gCL * capL + iL)
            const double hL = gCL * capL + iL;
            // condensateur u->B : i = gCH * (u - B) - (gCH * capH + iH)
            const double hH = gCH * capH + iH;
            // KCL A : gR (u - A) - (gCL A - hL) + gP (B - A) = 0
            // KCL B : gCH (u - B) - hH - gRh B + gP (A - B) = 0
            const double a11 = gR + gCL + gP, a12 = -gP, b1 = gR * u + hL;
            const double a21 = -gP, a22 = gCH + gRh + gP, b2 = gCH * u - hH;
            const double det = a11 * a22 - a12 * a21;
            const double A = (b1 * a22 - a12 * b2) / det;
            const double Bn = (a11 * b2 - a21 * b1) / det;
            const double iLn = gCL * A - hL;          capL = A;      iL = iLn;
            const double iHn = gCH * (u - Bn) - hH;   capH = u - Bn; iH = iHn;
            a = A; b = Bn;
            // curseur : diviseur le long du pot (charge du curseur negligeable : etage suivant a haute impedance)
            return A + (Bn - A) * pos;
        }
    private:
        double T = 1.0 / 48000.0, pos = 0.5, a = 0, b = 0, capL = 0, capH = 0, iL = 0, iH = 0;
    };
}
