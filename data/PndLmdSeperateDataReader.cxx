/*
 * PndLmdSeperateDataReader.cxx
 *
 *  Created on: Aug 26, 2013
 *      Author: steve
 */

#include "PndLmdSeperateDataReader.h"

#include "PndLmdTrackQ.h"
#include "PndSdsMCPoint.h"
#include "PndMCTrack.h"
#include "PndTrack.h"
#include "FairTrackParH.h"

PndLmdSeperateDataReader::PndLmdSeperateDataReader() :
		MC_tree("cbmsim"), track_tree("cbmsim"), geane_tree("pndsim"), combined_track_params(
				"PndLmdTrackQ", 1) {
}

PndLmdSeperateDataReader::~PndLmdSeperateDataReader() {
}

unsigned int PndLmdSeperateDataReader::getEntries() const {
	if (MC_tree.GetEntries() > 0
			&& MC_tree.GetEntries() < geane_tree.GetEntries())
		return MC_tree.GetEntries();
	return geane_tree.GetEntries();
}

void PndLmdSeperateDataReader::initDataStream() {
	for (unsigned int i = 0; i < file_paths.size(); i++) {
	//	MC_tree.Add(data_dirs[i] + "/Lumi_MC*.root");
	//	track_tree.Add(data_dirs[i] + "/Lumi_Track*.root");
	//	geane_tree.Add(data_dirs[i] + "/Lumi_Geane*.root");
	}

	MC_tree.SetBranchStatus("*", 0);
	track_tree.SetBranchStatus("*", 0);
	geane_tree.SetBranchStatus("*", 0);

	//--- MC info --------------------------------------------------------------------
	true_tracks = new TClonesArray("PndMCTrack");
	MC_tree.SetBranchAddress("MCTrack", &true_tracks); //True Track to compare
	MC_tree.SetBranchStatus("MCTrack*", 1);

	true_points = new TClonesArray("PndSdsMCPoint");
	MC_tree.SetBranchAddress("LMDPoint", &true_points); //True Points to compare
	MC_tree.SetBranchStatus("LMDPoint*", 1);
	//--------------------------------------------------------------------------------

	//--- Track info -----------------------------------------------------------------
	lmd_tracks = new TClonesArray("PndTrack");
	track_tree.SetBranchAddress("LMDPndTrack", &lmd_tracks); //Reco Track to compare (at lumi monitor system, so not backtracked)
	track_tree.SetBranchStatus("LMDPndTrack*", 1);
	//--------------------------------------------------------------------------------

	//--- Geane info -----------------------------------------------------------------
	geane_tracks = new TClonesArray("FairTrackParH");
	geane_tree.SetBranchAddress("GeaneTrackFinal", &geane_tracks); //Tracks with parabolic parametrisation
	geane_tree.SetBranchStatus("GeaneTrackFinal*", 1); //Tracks with parabolic parametrisation

	mc_entries = MC_tree.GetEntries();
	num_entries = mc_entries;
	lmd_track_entries = track_tree.GetEntries();
	if (lmd_track_entries < mc_entries && lmd_track_entries > 0)
		num_entries = lmd_track_entries;
	else if (geane_tree.GetEntries() < num_entries
			&& geane_tree.GetEntries() > 0)
		num_entries = geane_tree.GetEntries();
}

void PndLmdSeperateDataReader::clearDataStream() {
}

TClonesArray* PndLmdSeperateDataReader::getEntry(unsigned int i) {
	if (mc_entries > 0)
		MC_tree.GetEntry(i);
	if (lmd_track_entries > 0)
		track_tree.GetEntry(i);
	geane_tree.GetEntry(i);

	PndLmdTrackQ *trackq = (PndLmdTrackQ*) combined_track_params.ConstructedAt(
			0);

	trackq->SetTrkRecStatus(-1);

	for (int ik = 0; ik < true_points->GetEntriesFast(); ik++) {
		PndSdsMCPoint *lmd_point = (PndSdsMCPoint*) true_points->At(ik);
		TVector3 MomMC;
		TVector3 Pos(lmd_point->GetPosition());
		lmd_point->Momentum(MomMC);
		MomMC.RotateY(-0.040);

		if (-2212
				== ((PndMCTrack*) true_tracks->At(lmd_point->GetTrackID()))->GetPdgCode()) {
			trackq->SetMCpointLMD(Pos.X(), Pos.Y(), Pos.Z());
			trackq->SetMCmomLMD(MomMC.Theta(), MomMC.Phi(), MomMC.Mag());
		}
	}
	for (int ik = 0; ik < true_tracks->GetEntries(); ik++) {
		PndMCTrack *mctrk = (PndMCTrack*) true_tracks->At(ik);
		TLorentzVector lv_mc = mctrk->Get4Momentum();
		Int_t mcID = mctrk->GetPdgCode();
		TVector3 MomMC_all = mctrk->GetMomentum();
		TVector3 Pos(mctrk->GetStartVertex());

		if (mcID == -2212 && mctrk->IsGeneratorCreated()) {
			trackq->SetMCpoint(Pos.X(), Pos.Y(), Pos.Z());
			trackq->SetMCmom(MomMC_all.Theta(), MomMC_all.Phi(),
					MomMC_all.Mag());
		}
	}

	for (Int_t iN = 0; iN < lmd_tracks->GetEntriesFast(); iN++) {
		PndTrack *track = (PndTrack*) lmd_tracks->At(iN);
		FairTrackParP trackpar = track->GetParamFirst();
		TVector3 MomRec = trackpar.GetMomentum();

		trackq->SetLMDpoint(trackpar.GetOrigin().X(), trackpar.GetOrigin().Y(),
				trackpar.GetOrigin().Z());
		trackq->SetLMDdir(MomRec.Theta(), MomRec.Phi());
	}

	// loop over geane tracks
	for (Int_t iN = 0; iN < geane_tracks->GetEntries(); iN++) {
		///-- Read info about GEANE(reconstructed) tracks--------------------------
		FairTrackParH *fRes = (FairTrackParH*) geane_tracks->At(iN);
		if (fRes->GetLambda() != 0) {
			trackq->SetTrkRecStatus(0);
			TVector3 MomRec = fRes->GetMomentum();

			trackq->SetIPpoint(fRes->GetPosition().X(), fRes->GetPosition().Y(),
					fRes->GetPosition().Z());
			trackq->SetIPmom(MomRec.Theta(), MomRec.Phi(), MomRec.Mag());
		}
	}

	return &combined_track_params;
}

