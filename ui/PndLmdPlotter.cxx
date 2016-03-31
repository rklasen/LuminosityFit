/*
 * PndLmdResultPlotter.cxx
 *
 *  Created on: Mar 25, 2013
 *      Author: steve
 */

#include "PndLmdPlotter.h"

#include "ROOTPlotHelper.hpp"

#include "fit/data/ROOT/ROOTDataHelper.h"
#include "visualization/ModelVisualizationProperties1D.h"
#include "fit/PndLmdLumiFitResult.h"
#include "data/PndLmdAngularData.h"
#include "data/PndLmdAcceptance.h"
#include "PndLmdFitFacade.h"
#include "data/PndLmdFitDataBundle.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iosfwd>
#include <ostream>
#include <iomanip>

#include "boost/filesystem.hpp"   // includes all needed Boost.Filesystem declarations
#include "boost/regex.hpp"

#include "TMath.h"
#include "TGraphErrors.h"
#include "TGraphAsymmErrors.h"
#include "TMultiGraph.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TF1.h"
#include "TLatex.h"
#include "TString.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TLine.h"
#include "TLegend.h"
#include "TEfficiency.h"
#include "TExec.h"
#include "TStyle.h"
#include "TGaxis.h"
#include "TColor.h"
#include "TPaletteAxis.h"
#include "TROOT.h"

using boost::filesystem::path;
using boost::filesystem::directory_iterator;
using std::stringstream;
using std::cout;
using std::endl;

namespace LumiFit {

  PndLmdPlotter::PndLmdPlotter() {
  }

  PndLmdPlotter::~PndLmdPlotter() {
  }

  std::pair<double, double> PndLmdPlotter::calulateRelDiff(double value,
      double error, double ref) const {
    if (ref != 0.0)
      return std::make_pair(100.0 * (value - ref) / ref, 100.0 * error / ref);

    return std::make_pair(100.0 * (value - ref) / value, 100.0 * error / value);
  }

  std::vector<DataStructs::DimensionRange> PndLmdPlotter::generatePlotRange(
      const PndLmdAbstractData &lmd_abs_data,
      const EstimatorOptions &est_options) const {
    PndLmdFitFacade fit_facade;

    std::vector<DataStructs::DimensionRange> ranges = fit_facade.calcRange(
        lmd_abs_data, est_options);

    if (primary_dimension_plot_range.is_active
        && lmd_abs_data.getPrimaryDimension().is_active) {
      ranges[0].range_low = primary_dimension_plot_range.range_low;
      ranges[0].range_high = primary_dimension_plot_range.range_high;
    }
    return ranges;
  }

  std::string PndLmdPlotter::makeDirName(
      const PndLmdAbstractData &elastic_bundle) const {
    boost::property_tree::ptree sim_prop =
        elastic_bundle.getSimulationParametersPropertyTree();

    std::stringstream ss;
    ss << "IP_pos_" << sim_prop.get<double>("ip_mean_x") << "_"
        << sim_prop.get<double>("ip_standard_deviation_x") << "_"
        << sim_prop.get<double>("ip_mean_y") << "_"
        << sim_prop.get<double>("ip_standard_deviation_y") << "_"
        << sim_prop.get<double>("ip_mean_z") << "_"
        << sim_prop.get<double>("ip_standard_deviation_z") << "-beam_tilt_"
        << sim_prop.get<double>("beam_tilt_x") << "_"
        << sim_prop.get<double>("beam_divergence_x") << "_"
        << sim_prop.get<double>("beam_tilt_y") << "_"
        << sim_prop.get<double>("beam_divergence_y");
    return ss.str();
  }

// TODO: this whole block of functions has to go somewhere else
  double PndLmdPlotter::getLuminosity(
      const ModelFitResult& model_fit_result) const {
    return model_fit_result.getFitParameter("luminosity").value;
  }
  double PndLmdPlotter::getLuminositySysError(
      const ModelFitResult& model_fit_result) const {
    return 0.0; // TODO
  }
  double PndLmdPlotter::getLuminosityStatError(
      const ModelFitResult& model_fit_result) const {
    return model_fit_result.getFitParameter("luminosity").error;
  }
  double PndLmdPlotter::getLuminosityError(
      const ModelFitResult& model_fit_result) const {
    return getLuminositySysError(model_fit_result)
        + getLuminosityStatError(model_fit_result);
  }
  double PndLmdPlotter::getRedChiSquare(
      const ModelFitResult& model_fit_result) const {
    return model_fit_result.getFinalEstimatorValue() / model_fit_result.getNDF();
  }

  std::pair<ModelVisualizationProperties1D, ModelVisualizationProperties1D> PndLmdPlotter::create2DVisualizationProperties(
      const EstimatorOptions &est_options,
      const PndLmdAbstractData &lmd_abs_data) const {
    // in case we want to make a difference graph in 2d the binnings have to match
    // hence if the fit range is shortened also the binning has to be reduced accordingly

    // ok just evaluate the function at 500 points in the range (is default);
    ModelVisualizationProperties1D vis_prop1;
    ModelVisualizationProperties1D vis_prop2;

    vis_prop1.setEvaluations(lmd_abs_data.getPrimaryDimension().bins);
    vis_prop1.setBinningFactor(lmd_abs_data.getPrimaryDimension().bin_size);
    DataStructs::DimensionRange dim_range(
        lmd_abs_data.getPrimaryDimension().dimension_range.getRangeLow(),
        lmd_abs_data.getPrimaryDimension().dimension_range.getRangeHigh());
    vis_prop1.setPlotRange(dim_range);

    vis_prop2.setEvaluations(lmd_abs_data.getSecondaryDimension().bins);
    vis_prop2.setBinningFactor(lmd_abs_data.getSecondaryDimension().bin_size);
    DataStructs::DimensionRange dim_range2(
        lmd_abs_data.getSecondaryDimension().dimension_range.getRangeLow(),
        lmd_abs_data.getSecondaryDimension().dimension_range.getRangeHigh());
    vis_prop2.setPlotRange(dim_range2);

    return std::make_pair(vis_prop1, vis_prop2);
  }

  ModelVisualizationProperties1D PndLmdPlotter::create1DVisualizationProperties(
      const EstimatorOptions &est_options,
      const PndLmdAbstractData &lmd_abs_data) const {
// ok just evaluate the function at 500 points in the range (is default);
    ModelVisualizationProperties1D vis_prop;

    vis_prop.setBinningFactor(lmd_abs_data.getBinningFactor(1));
    DataStructs::DimensionRange plot_range = generatePlotRange(lmd_abs_data,
        est_options)[0];
    vis_prop.setPlotRange(plot_range);

    return vis_prop;
  }

// histogram and graph creation from lmd objects

  void PndLmdPlotter::setCurrentFitDataBundle(
      const PndLmdFitDataBundle &fit_data_bundle) {
    current_fit_bundle = fit_data_bundle;
  }

  TH2D* PndLmdPlotter::create2DHistogramFromFitResult(
      const PndLmdFitOptions &fit_opt,
      const PndLmdElasticDataBundle &elastic_data_bundle) {

    // set the appropriate acceptance and resolutions in the factory
    // in principle there could be multiple acceptances used here but we assume there is just 1
    if (elastic_data_bundle.getUsedAcceptanceIndices().size() > 0) {
      unsigned int used_acceptance_index =
          elastic_data_bundle.getUsedAcceptanceIndices()[0];
      lmd_fit_facade.setModelFactoryAcceptence(
          current_fit_bundle.getUsedAcceptancesPool()[used_acceptance_index]);
    }

    if (elastic_data_bundle.getUsedResolutionsIndexRanges().size() > 0) {
      std::vector<PndLmdHistogramData> used_resolutions;
      for (unsigned int res_index_range = 0;
          res_index_range
              < elastic_data_bundle.getUsedResolutionsIndexRanges().size();
          ++res_index_range) {
        used_resolutions.insert(used_resolutions.end(),
            current_fit_bundle.getUsedResolutionsPool().begin()
                + elastic_data_bundle.getUsedResolutionsIndexRanges()[res_index_range].first,
            current_fit_bundle.getUsedResolutionsPool().begin()
                + elastic_data_bundle.getUsedResolutionsIndexRanges()[res_index_range].second);
      }
      //model_factory.setResolutions(used_resolutions);

      lmd_fit_facade.setModelFactoryResolutions(used_resolutions);
    }

    shared_ptr<Model> model = lmd_fit_facade.generateModel(elastic_data_bundle,
        fit_opt);
    lmd_fit_facade.initBeamParametersForModel(model,
        fit_opt.getModelOptionsPropertyTree());
    //shared_ptr<Model> model = model_factory.generateModel(
    //		fit_opt.getModelOptionsPropertyTree(), elastic_data_bundle);

    // now just overwrite all parameters in the model from the fit result
    model->getModelParameterHandler().initModelParametersFromFitResult(
        elastic_data_bundle.getFitResult(fit_opt));

    model->getModelParameterSet().printInfo();
    if (model->init()) {
      std::cout << "Error: not all parameters have been set!" << std::endl;
    }
    // trigger model calculations
    model->updateModel();

    const std::set<ModelStructs::minimization_parameter> &fit_params =
        elastic_data_bundle.getFitResult(fit_opt).getFitParameters();
    std::set<ModelStructs::minimization_parameter>::const_iterator fit_param_it;

    for (fit_param_it = fit_params.begin(); fit_param_it != fit_params.end();
        fit_param_it++) {
      std::cout << fit_param_it->name.first << " : "
          << fit_param_it->name.second << " = " << fit_param_it->value << " +- "
          << fit_param_it->error << std::endl;
    }

    std::pair<ModelVisualizationProperties1D, ModelVisualizationProperties1D> vis_props =
        create2DVisualizationProperties(fit_opt.getEstimatorOptions(),
            elastic_data_bundle);

    TH2D* hist = root_plotter.createHistogramFromModel2D(model, vis_props);

    return hist;
  }

  TGraphAsymmErrors* PndLmdPlotter::createGraphFromFitResult(
      const PndLmdFitOptions &fit_opt, PndLmdAngularData &data) const {

    shared_ptr<Model> model = model_factory.generateModel(
        fit_opt.getModelOptionsPropertyTree(), data);
    if (model->init()) {
      std::cout << "Error: not all parameters have been set!" << std::endl;
    }

// now just overwrite all parameters in the model from the fit result
    model->getModelParameterHandler().initModelParametersFromFitResult(
        data.getFitResult(fit_opt));

    ModelVisualizationProperties1D vis_prop = create1DVisualizationProperties(
        fit_opt.getEstimatorOptions(), data);

    return root_plotter.createGraphFromModel1D(model, vis_prop);
  }

  TGraphAsymmErrors* PndLmdPlotter::createVertexGraphFromFitResult(
      const PndLmdFitOptions &fit_opt, const PndLmdHistogramData &data) const {

    shared_ptr<Model1D> model = model_factory.generate1DVertexModel(
        fit_opt.getModelOptionsPropertyTree());
    if (model->init()) {
      std::cout << "Error: not all parameters have been set!" << std::endl;
    }

// now just overwrite all parameters in the model from the fit result
    model->getModelParameterHandler().initModelParametersFromFitResult(
        data.getFitResult(fit_opt));

    ModelVisualizationProperties1D vis_prop = create1DVisualizationProperties(
        fit_opt.getEstimatorOptions(), data);

    return root_plotter.createGraphFromModel1D(model, vis_prop);
  }

  /*TH2D* PndLmdPlotter::create2DHistogramFromFitResult(
   const PndLmdFitOptions &fit_opt,
   const PndLmdHistogramData &res_data) const {

   PndLmdModelFactory model_factory;
   shared_ptr<Model2D> model = model_factory.generate2DResolutionModel(
   fit_opt);
   if (model->init()) {
   std::cout << "Error: not all parameters have been set!" << std::endl;
   }

   // now just overwrite all parameters in the model from the fit result
   model->getModelParameterHandler().initModelParametersFromFitResult(
   res_data.getFitResult(fit_opt).getModelFitResult());

   std::pair<ModelVisualizationProperties1D, ModelVisualizationProperties1D> vis_props =
   create2DVisualizationProperties(fit_opt.getEstimatorOptions(), res_data);

   TH2D* hist = root_plotter.createHistogramFromModel2D(model, vis_props);

   return hist;
   }

   TGraphAsymmErrors* PndLmdPlotter::createSmearingGraphFromFitResult(
   const PndLmdFitOptions &fit_opt,
   const PndLmdHistogramData &res_data) const {
   PndLmdModelFactory model_factory;
   shared_ptr<Model1D> model = model_factory.generate1DResolutionModel(
   fit_opt.getFitModelOptions());
   if (model->init()) {
   std::cout << "Error: not all parameters have been set!" << std::endl;
   }

   // now just overwrite all parameters in the model from the fit result
   model->getModelParameterHandler().initModelParametersFromFitResult(
   res_data.getFitResult(fit_opt).getModelFitResult());

   // all parameters of the smearing model have to be freed otherwise the
   // parametrization model will overwrite the values during the evaluation
   model->getModelParameterSet().freeAllModelParameters();

   ModelVisualizationProperties1D vis_prop = create1DVisualizationProperties(
   fit_opt.getEstimatorOptions(), res_data);

   return root_plotter.createGraphFromModel1D(model, vis_prop);
   }*/

  /*
   std::pair<TGraphAsymmErrors*, TGraphAsymmErrors*> PndLmdPlotter::createResidual(
   const PndLmdLumiFitOptions &fit_opt, PndLmdAngularData &data) {
   const TH1D *data_hist = data.get1DHistogram();

   PndLmdModelFactory model_factory;

   shared_ptr<Model1D> model = model_factory.generate1DModel(
   fit_opt.getFitModelOptions(), data.getLabMomentum());
   if (model->init()) {
   std::cout << "Error: not all parameters have been set!" << std::endl;
   }

   // now just overwrite all parameters in the model from the fit result
   model->getModelParameterHandler().initModelParametersFromFitResult(
   data.getFitResult(fit_opt)->getModelFitResult());

   int counter = 0;
   double xvals[data_hist->GetNbinsX()];
   double yvals[data_hist->GetNbinsX()];
   double yerrs[data_hist->GetNbinsX()];
   double model_yvals[data_hist->GetNbinsX()];
   double model_errs_low[data_hist->GetNbinsX()];
   double model_errs_high[data_hist->GetNbinsX()];

   for (unsigned int i = 0; i < data_hist->GetNbinsX(); i++) {
   double eval_point = data_hist->GetBinCenter(i);
   if (!fit_opt.getEstimatorOptions().getFitRangeX().isDataWithinRange(
   eval_point)) {
   continue;
   }
   xvals[counter] = eval_point;
   yvals[counter] = data_hist->GetBinContent(i)
   - model->evaluate(&eval_point) * data.getBinningFactor(1);
   model_yvals[counter] = 0.0;
   yerrs[counter] = data_hist->GetBinError(i);

   if (yvals[counter] > 0.0) {
   model_errs_low[counter] = model->getUncertaincy(&eval_point).second
   * data.getBinningFactor(1);
   model_errs_high[counter] = model->getUncertaincy(&eval_point).first
   * data.getBinningFactor(1);
   } else {
   model_errs_low[counter] = model->getUncertaincy(&eval_point).first
   * data.getBinningFactor(1);
   model_errs_high[counter] = model->getUncertaincy(&eval_point).second
   * data.getBinningFactor(1);
   }
   counter++;
   }
   return std::make_pair(
   new TGraphAsymmErrors(counter, xvals, yvals, 0, 0, yerrs, yerrs),
   new TGraphAsymmErrors(counter, xvals, model_yvals, 0, 0, model_errs_low,
   model_errs_high));
   }*/

  TGraphAsymmErrors* PndLmdPlotter::createAcceptanceGraph(
      const PndLmdAcceptance *acc) const {
    TVirtualPad *current_pad = gPad;

    TEfficiency *eff = acc->getAcceptance1D(); // false = angular acceptance
    TCanvas c;
    eff->Draw();
    c.Update();

    gPad = current_pad; // reset the pad to previous
    return new TGraphAsymmErrors(*eff->GetPaintedGraph());
  }

  TH2D* PndLmdPlotter::createAcceptanceHistogram(
      const PndLmdAcceptance& acc) const {
    TVirtualPad *current_pad = gPad;

    TEfficiency *eff = acc.getAcceptance2D(); // false = angular acceptance
    TCanvas c;
    eff->Draw("colz");
    c.Update();

    gPad = current_pad; // reset the pad to previous
    return new TH2D(*(TH2D*) eff->GetPaintedHistogram());
  }

  TGraphAsymmErrors* PndLmdPlotter::generateDPMModelPartGraph(double plab,
      LumiFit::DPMElasticParts dpm_elastic_part,
      DataStructs::DimensionRange& plot_range) {

    ModelVisualizationProperties1D vis_prop;
    vis_prop.setPlotRange(plot_range);

    boost::property_tree::ptree model_opt_ptree;
    model_opt_ptree.put("dpm_elastic_parts",
        LumiFit::DPMElasticPartsToString.at(dpm_elastic_part));
    model_opt_ptree.put("fit_dimension", 1);

    PndLmdAngularData data;
    data.setLabMomentum(plab);

    shared_ptr<Model> model = model_factory.generateModel(model_opt_ptree,
        data);
    if (model->init()) {
      std::cout << "Error: not all parameters have been set!" << std::endl;
    }

    TGraphAsymmErrors* model_graph = root_plotter.createGraphFromModel1D(model,
        vis_prop);

    return model_graph;
  }

// special plots which are a combination of several data objects
  TGraphAsymmErrors* PndLmdPlotter::makeXYOverviewGraph(
      const std::vector<PndLmdHistogramData> &data_vec,
      double error_scaling_factor) const {
    std::vector<NeatPlotting::GraphPoint> graph_data;

    NeatPlotting::GraphPoint current_graph_point;

    // first group all vertex data plots of some specific ip property setting together
    std::map<std::pair<double, double>, std::vector<PndLmdHistogramData> > xy_ip_data_map;
    for (unsigned int i = 0; i < data_vec.size(); i++) {

      boost::property_tree::ptree sim_params(
          data_vec[i].getSimulationParametersPropertyTree());
      double ip_mean_x = sim_params.get<double>("ip_mean_x");
      double ip_mean_y = sim_params.get<double>("ip_mean_y");

      xy_ip_data_map[std::make_pair(ip_mean_x, ip_mean_y)].push_back(
          data_vec[i]);
    }

    std::map<std::pair<double, double>, std::vector<PndLmdHistogramData> >::const_iterator ip_setting_case;
    for (ip_setting_case = xy_ip_data_map.begin();
        ip_setting_case != xy_ip_data_map.end(); ip_setting_case++) {

      current_graph_point.x = ip_setting_case->first.first * 10; // *10 from cm to mm
      current_graph_point.y = ip_setting_case->first.second * 10; // *10 from cm to mm
      current_graph_point.x_err_low = 0.0;
      current_graph_point.x_err_high = 0.0;
      current_graph_point.y_err_low = 0.0;
      current_graph_point.y_err_high = 0.0;

      std::cout << "vertex data count for " << ip_setting_case->first.first
          << ";" << ip_setting_case->first.second << ": "
          << ip_setting_case->second.size() << std::endl;

      for (unsigned int i = 0; i < ip_setting_case->second.size(); i++) {
        if (ip_setting_case->second[i].getPrimaryDimension().dimension_options.dimension_type
            == LumiFit::X) {
          ModelFitResult fit_result =
              ip_setting_case->second[i].getFitResults().begin()->second;

          if (fit_result.getFitParameters().size() > 0) {
            double diff = error_scaling_factor
                * (fit_result.getFitParameter("gauss_mean").value * 10
                    - current_graph_point.x);
            double absdiff = fabs(diff);

            std::cout << diff << " " << absdiff << std::endl;
            //	if (absdiff > 0.1 * max_offset && absdiff < max_offset) {
            if (diff < 0.0)
              current_graph_point.x_err_low = absdiff;
            else
              current_graph_point.x_err_high = absdiff;
            //	}
          }
        } else if (ip_setting_case->second[i].getPrimaryDimension().dimension_options.dimension_type
            == LumiFit::Y) {
          ModelFitResult fit_result =
              ip_setting_case->second[i].getFitResults().begin()->second;

          if (fit_result.getFitParameters().size() > 0) {
            double diff = error_scaling_factor
                * (fit_result.getFitParameter("gauss_mean").value * 10
                    - current_graph_point.y);
            double absdiff = fabs(diff);

            std::cout << diff << " " << absdiff << std::endl;
            //	if (absdiff > 0.1 * max_offset && absdiff < max_offset) {
            if (diff < 0.0)
              current_graph_point.y_err_low = absdiff;
            else
              current_graph_point.y_err_high = absdiff;
            //	}
          }
        }
      }

      // add values to vector
      graph_data.push_back(current_graph_point);
    }

    NeatPlotting::GraphAndHistogramHelper graph_helper;
    return graph_helper.makeGraph(graph_data);
  }

// functions creating NeatPlotting::PlotBundles from lmd data objects

  void PndLmdPlotter::applyPlotRanges(
      NeatPlotting::PlotBundle &plot_bundle) const {
    if (primary_dimension_plot_range.is_active) {
      if (primary_dimension_plot_range.range_low
          < primary_dimension_plot_range.range_high) {
        plot_bundle.plot_axis.x_axis_range.low =
            primary_dimension_plot_range.range_low;
        plot_bundle.plot_axis.x_axis_range.high =
            primary_dimension_plot_range.range_high;
        plot_bundle.plot_axis.x_axis_range.active = true;
      }
    }
    if (secondary_dimension_plot_range.is_active) {
      if (secondary_dimension_plot_range.range_low
          < secondary_dimension_plot_range.range_high) {
        plot_bundle.plot_axis.y_axis_range.low =
            secondary_dimension_plot_range.range_low;
        plot_bundle.plot_axis.y_axis_range.high =
            secondary_dimension_plot_range.range_high;
        plot_bundle.plot_axis.y_axis_range.active = true;
      }
    }
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeAcceptanceBundle1D(
      const PndLmdAcceptance& acc) const {
    NeatPlotting::PlotBundle acc_bundle;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "PE";

    style.line_style.line_color = kRed;
    style.marker_style.marker_color = kRed;
    style.marker_style.marker_style = 20;

    NeatPlotting::GraphAndHistogramHelper hist_helper;
    acc_bundle.addGraph(
        hist_helper.rescaleAxis(createAcceptanceGraph(&acc), 1000.0), style);

    acc_bundle.plot_axis.x_axis_title =
        acc.getPrimaryDimension().createAxisLabel();
    acc_bundle.plot_axis.y_axis_title = "efficiency";

    // add full efficiency line
    acc_bundle.plot_decoration.x_parallel_lines.push_back(1.0);

    applyPlotRanges(acc_bundle);
    return acc_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeAcceptanceBundle2D(
      const PndLmdAcceptance& acc) const {
    NeatPlotting::PlotBundle acc_bundle;

    TVirtualPad *current_pad = gPad;

    TEfficiency *eff = acc.getAcceptance2D();
    TCanvas c;
    eff->Draw("COLZ");
    c.Update();

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "COL";

    NeatPlotting::GraphAndHistogramHelper hist_helper;
    acc_bundle.addHistogram(
        hist_helper.rescaleAxis((TH2D*) eff->GetPaintedHistogram(), 1000.0,
            1000.0), style);

    acc_bundle.plot_axis.x_axis_title =
        acc.getPrimaryDimension().createDimensionLabel() + " /m"
            + acc.getPrimaryDimension().createUnitLabel();
    acc_bundle.plot_axis.y_axis_title =
        acc.getSecondaryDimension().createDimensionLabel() + " /m"
            + acc.getSecondaryDimension().createUnitLabel();
    acc_bundle.plot_axis.z_axis_title = "efficiency";

    gPad = current_pad; // reset the pad to the previous

    applyPlotRanges(acc_bundle);
    return acc_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeGraphBundle(
      PndLmdAngularData& data, const PndLmdFitOptions &fit_options,
      bool draw_data, bool draw_model, bool draw_labels) {
    std::cout << "Creating graph bundle!" << std::endl;

    NeatPlotting::PlotBundle ang_plot_bundle;

    unsigned int fit_dimension = fit_options.getModelOptionsPropertyTree().get<
        unsigned int>("fit_dimension");

    if (fit_dimension == 1) {
      NeatPlotting::DataObjectStyle style;
      style.draw_option = "E1";

      if (draw_data) {
        TH1D* hist = data.get1DHistogram();
        ang_plot_bundle.addHistogram(hist, style);

        std::stringstream ss;
        if (data.getPrimaryDimension().dimension_options.dimension_type
            == LumiFit::T) {
          if (primary_dimension_plot_range.range_low
              < primary_dimension_plot_range.range_high
              && primary_dimension_plot_range.is_active) {
            DataStructs::DimensionRange temp_primary_dimension_plot_range =
                primary_dimension_plot_range;

            primary_dimension_plot_range.range_low =
                -model_factory.getMomentumTransferFromTheta(
                    data.getLabMomentum(),
                    primary_dimension_plot_range.range_low);
            primary_dimension_plot_range.range_high =
                -model_factory.getMomentumTransferFromTheta(
                    data.getLabMomentum(),
                    primary_dimension_plot_range.range_high);
            applyPlotRanges(ang_plot_bundle);
            primary_dimension_plot_range = temp_primary_dimension_plot_range;
          }
          ss << "# of tracks / " << hist->GetXaxis()->GetBinWidth(1)
              << " GeV^{2}/c^{2}";
        } else {
          applyPlotRanges(ang_plot_bundle);
          ang_plot_bundle.plot_axis.x_axis_title =
              data.getPrimaryDimension().createAxisLabel();
          ss << "# of tracks / " << 1000.0 * hist->GetXaxis()->GetBinWidth(1)
              << " m" << data.getPrimaryDimension().createUnitLabel();
        }
        ang_plot_bundle.plot_axis.y_axis_title = ss.str();
      }
      if (draw_model) {
        ModelFitResult fit_res = data.getFitResult(fit_options);

        if (fit_dimension == 1) {
          if (0 == fit_res.getFitStatus()) {
            style.draw_option = "C";
            style.line_style.line_color = kRed;
            style.marker_style.marker_color = kRed;
            ang_plot_bundle.addGraph(
                createGraphFromFitResult(fit_options, data), style);
          }
        }
      }
    }
    if (fit_dimension == 2) {
      NeatPlotting::DataObjectStyle style;
      style.draw_option = "COL";

      TH2D* hist = data.get2DHistogram();
      NeatPlotting::GraphAndHistogramHelper hist_helper;
      hist = hist_helper.rescaleAxis(hist, 1000.0, 1000.0);

      ang_plot_bundle.plot_axis.x_axis_title =
          data.getPrimaryDimension().createDimensionLabel() + " /m"
              + data.getPrimaryDimension().createUnitLabel();
      ang_plot_bundle.plot_axis.y_axis_title =
          data.getSecondaryDimension().createDimensionLabel() + " /m"
              + data.getSecondaryDimension().createUnitLabel();

      std::stringstream ss;
      ss << "# of tracks / "
          << hist->GetXaxis()->GetBinWidth(1)
              * hist->GetYaxis()->GetBinWidth(1);
      ang_plot_bundle.plot_axis.z_axis_title = ss.str();

      TH2D* model(0);

      if (draw_model) {
        ModelFitResult fit_res = data.getFitResult(fit_options);

        std::cout << fit_res.getFitStatus() << " "
            << fit_res.getFitParameters().size() << std::endl;
        if (0 == fit_res.getFitStatus()) {
          model = create2DHistogramFromFitResult(fit_options, data);
          model = hist_helper.rescaleAxis(model, 1000.0, 1000.0);
        }
      }

      if (draw_data && draw_model) {
        TH2D* diff = neat_plot_helper.makeDifferenceHistogram(hist, model);
        ang_plot_bundle.addHistogram(diff, style);
      } else {
        if (draw_data)
          ang_plot_bundle.addHistogram(hist, style);

        if (draw_model && model)
          ang_plot_bundle.addHistogram(model, style);
      }
      applyPlotRanges(ang_plot_bundle);
    }

    if (draw_labels) {
      stringstream strstream;
      strstream.precision(3);

      strstream << "p_{lab} = " << data.getLabMomentum() << " GeV";

      NeatPlotting::TextStyle text_style;
      NeatPlotting::PlotLabel plot_label(strstream.str(), text_style);
      ang_plot_bundle.plot_decoration.label_text_leftpos = 0.7;
      ang_plot_bundle.plot_decoration.label_text_toppos = 1.0;
      ang_plot_bundle.plot_decoration.label_text_spacing = 0.06;
      ang_plot_bundle.plot_decoration.use_line_layout = true;
      ang_plot_bundle.plot_decoration.labels.push_back(plot_label);
      strstream.str("");

      if (draw_model) {
        double lumi_ref = data.getReferenceLuminosity();

        PndLmdLumiFitResult fit_res;
        fit_res.setModelFitResult(data.getFitResult(fit_options));

        if (fit_res.getModelFitResult().getFitStatus() == 0) {
          strstream << std::setprecision(3) << "#Delta L/L = "
              << calulateRelDiff(fit_res.getLuminosity(),
                  fit_res.getLuminosityError(), lumi_ref).first << " #pm "
              << calulateRelDiff(fit_res.getLuminosity(),
                  fit_res.getLuminosityError(), lumi_ref).second << " %";
          plot_label.setTitle(strstream.str());
          ang_plot_bundle.plot_decoration.labels.push_back(plot_label);
          strstream.str("");

          if (draw_data) {
            const std::set<ModelStructs::minimization_parameter> fit_params =
                fit_res.getModelFitResult().getFitParameters();
            std::set<ModelStructs::minimization_parameter>::const_iterator fit_param_it;
            for (fit_param_it = fit_params.begin();
                fit_param_it != fit_params.end(); fit_param_it++) {
              strstream.str("");
              strstream << std::setprecision(3) << fit_param_it->name.second
                  << "= " << fit_param_it->value << " #pm "
                  << fit_param_it->error;
              plot_label.setTitle(strstream.str());
              ang_plot_bundle.plot_decoration.labels.push_back(plot_label);
            }
          }
        }
      }
    }

    return ang_plot_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeResidualPlotBundle1D(
      PndLmdAngularData& data, const PndLmdFitOptions &fit_options) {
    NeatPlotting::PlotBundle residual_plot_bundle;

    unsigned int fit_dimension = fit_options.getModelOptionsPropertyTree().get<
        unsigned int>("fit_dimension");

    if (fit_dimension != 1) {
      return residual_plot_bundle;
    }

    NeatPlotting::DataObjectStyle data_style;
    data_style.draw_option = "PE";
    data_style.marker_style.marker_style = 20;

    TH1D* hist = data.get1DHistogram();
    TGraphAsymmErrors *graph(0);

    ModelFitResult fit_res = data.getFitResult(fit_options);
    if (0 == fit_res.getFitStatus()) {
      graph = createGraphFromFitResult(fit_options, data);
    }

    bool momentum_transfer_active(
        fit_options.getModelOptionsPropertyTree().get<bool>(
            "momentum_transfer_active"));

    std::stringstream ss;
    if (momentum_transfer_active) {
      if (primary_dimension_plot_range.range_low
          < primary_dimension_plot_range.range_high
          && primary_dimension_plot_range.is_active) {
        DataStructs::DimensionRange temp_primary_dimension_plot_range =
            primary_dimension_plot_range;

        primary_dimension_plot_range.range_low =
            -model_factory.getMomentumTransferFromTheta(data.getLabMomentum(),
                primary_dimension_plot_range.range_low);
        primary_dimension_plot_range.range_high =
            -model_factory.getMomentumTransferFromTheta(data.getLabMomentum(),
                primary_dimension_plot_range.range_high);
        applyPlotRanges(residual_plot_bundle);
        primary_dimension_plot_range = temp_primary_dimension_plot_range;
      }
      ss << "data - model";
    } else {
      applyPlotRanges(residual_plot_bundle);
      residual_plot_bundle.plot_axis.x_axis_title =
          data.getPrimaryDimension().createAxisLabel();
      ss << "data - model";
    }
    residual_plot_bundle.plot_axis.y_axis_title = ss.str();

    if (hist && graph) {
      TGraphAsymmErrors *residual = neat_plot_helper.makeDifferenceGraph(hist,
          graph);
      residual_plot_bundle.addGraph(residual, data_style);

      data_style.draw_option = "C";
      data_style.line_style.line_color = kRed;
      data_style.marker_style.marker_color = kRed;

      TGraphAsymmErrors *zero_line_graph =
          neat_plot_helper.makeResidualZeroGraph(graph);
      residual_plot_bundle.addGraph(residual, data_style);
    }

    return residual_plot_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeResolutionGraphBundle(
      const PndLmdHistogramData & res) const {
    cout << "Creating resolution graph bundle!" << endl;

    NeatPlotting::GraphAndHistogramHelper hist_helper;

    NeatPlotting::PlotBundle res_plot_bundle;

    NeatPlotting::DataObjectStyle style;

    TH2D* hist2d;
    double scaling_factor_to_mrad(1000.0);
    if (res.getSecondaryDimension().is_active) {
      hist2d = res.get2DHistogram();
      style.draw_option = "COL";
      res_plot_bundle.addHistogram(
          hist_helper.rescaleAxis(hist2d, scaling_factor_to_mrad,
              scaling_factor_to_mrad), style);
      res_plot_bundle.plot_axis.x_axis_title =
          res.getPrimaryDimension().createDimensionLabel() + " /m"
              + res.getPrimaryDimension().createUnitLabel();
      res_plot_bundle.plot_axis.y_axis_title =
          res.getSecondaryDimension().createDimensionLabel() + " /m"
              + res.getSecondaryDimension().createUnitLabel();
      std::stringstream zaxis_label;
      zaxis_label.precision(2);
      zaxis_label << "# rec tracks / " << std::fixed
          << 1000.0 * hist2d->GetXaxis()->GetBinWidth(1) * 1000.0
              * hist2d->GetYaxis()->GetBinWidth(1) << " mrad^{2}";
      res_plot_bundle.plot_axis.z_axis_title = zaxis_label.str();

    } else {
      TH1D* hist1d = res.get1DHistogram();
      style.draw_option = "E1";
      res_plot_bundle.addHistogram(hist1d, style);
      res_plot_bundle.plot_axis.x_axis_title =
          res.getPrimaryDimension().createAxisLabel();
      std::stringstream yaxis_label;
      yaxis_label.precision(2);
      yaxis_label << "# rec tracks / " << std::fixed
          << 1000.0 * hist1d->GetXaxis()->GetBinWidth(1) << " mrad";
      res_plot_bundle.plot_axis.y_axis_title = yaxis_label.str();
    }

    stringstream strstream;
    strstream.precision(3);

    strstream << "p_{lab} = " << res.getLabMomentum() << " GeV";
    NeatPlotting::TextStyle text_style;
    NeatPlotting::PlotLabel plot_label(strstream.str(), text_style);
    res_plot_bundle.plot_decoration.label_text_leftpos = 0.7;
    res_plot_bundle.plot_decoration.label_text_toppos = 0.95;
    res_plot_bundle.plot_decoration.label_text_spacing = 0.06;
    res_plot_bundle.plot_decoration.use_line_layout = true;
    res_plot_bundle.plot_decoration.labels.push_back(plot_label);
    strstream.str("");

    if (res.getSelectorSet().size() > 0) {
      bool found_phi_selection(false);
      const std::set<LumiFit::LmdDimension> &selection_set =
          res.getSelectorSet();
      std::set<LumiFit::LmdDimension>::const_iterator selection_set_it;
      for (selection_set_it = selection_set.begin();
          selection_set_it != selection_set.end(); selection_set_it++) {
        if (selection_set_it->dimension_options.dimension_type
            == LumiFit::THETA) {
          found_phi_selection = true;
          break;
        }
      }
      if (found_phi_selection) {
        strstream << "#theta_{MC} = "
            << 1000 * selection_set_it->dimension_range.getRangeLow() << "-"
            << 1000 * selection_set_it->dimension_range.getRangeHigh()
            << " mrad";
        plot_label.setTitle(strstream.str());
        res_plot_bundle.plot_decoration.labels.push_back(plot_label);
      }
    }

    applyPlotRanges(res_plot_bundle);

    return res_plot_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeVertexGraphBundle1D(
      const PndLmdHistogramData & data) const {
    std::cout << "Creating vertex graph bundle!" << std::endl;

    const map<PndLmdFitOptions, ModelFitResult>& fit_results =
        data.getFitResults();

    NeatPlotting::PlotBundle ver_plot_bundle;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "E1";

    TH1D* hist = data.get1DHistogram();
    ver_plot_bundle.addHistogram(hist, style);

    ver_plot_bundle.plot_axis.x_axis_title =
        data.getPrimaryDimension().createAxisLabel();

    std::stringstream ss;
    ss << "# of tracks / " << hist->GetXaxis()->GetBinWidth(1) << " cm";
    ver_plot_bundle.plot_axis.y_axis_title = ss.str();

    std::cout << "num fit results: " << fit_results.size() << std::endl;
    if (fit_results.size() > 0) {
      if (fit_results.begin()->second.getFitStatus() == 0) {
        style.draw_option = "PE";
        style.line_style.line_color = kRed;
        style.marker_style.marker_color = kRed;
        style.marker_style.marker_style = 20;
        ver_plot_bundle.addGraph(
            createVertexGraphFromFitResult(fit_results.begin()->first, data),
            style);
      }
    }

    applyPlotRanges(ver_plot_bundle);
    return ver_plot_bundle;
  }

  /*std::map<PndLmdFitOptions, NeatPlotting::PlotBundle> PndLmdPlotter::makeGraphBundles1D(
   std::vector<PndLmdAngularData> &data_vec,
   LumiFit::PndLmdFitModelOptions & fitop) {

   std::map<PndLmdLumiFitOptions, NeatPlotting::PlotBundle> return_map;

   for (std::vector<PndLmdAngularData>::iterator data = data_vec.begin();
   data != data_vec.end(); data++) {
   // we only want IP info, so throw out everything else
   if (data->getPrimaryDimension().dimension_options.track_param_type
   != LumiFit::IP) {
   continue;
   }

   // loop over fit results
   map<PndLmdLumiFitOptions, PndLmdLumiFitResult*> fit_results =
   data->getFitResults();
   for (map<PndLmdLumiFitOptions, PndLmdLumiFitResult*>::iterator it =
   fit_results.begin(); it != fit_results.end(); it++) {

   if (it->first.getFitModelOptions().equalBinaryOptions(fitop)) {
   return_map[it->first] = makeGraphBundle1D(*data, it->first);
   }
   }
   }
   return return_map;
   }*/

  NeatPlotting::PlotBundle PndLmdPlotter::makeIPXYOverviewGraphBundle(
      const std::vector<PndLmdHistogramData> &vertex_data) const {

    NeatPlotting::PlotBundle plot_bundle;

// this should be made a bit better...
    unsigned int num_values = 1;
    double scales[] = { 20.0 };
    int colors[] = { kRed };

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "PE";

    for (unsigned int i = 0; i < num_values; i++) {
      TGraphAsymmErrors *gae = makeXYOverviewGraph(vertex_data, scales[i]);

      style.line_style.line_color = colors[i];
      style.marker_style.marker_color = colors[i];
      plot_bundle.addGraph(gae, style);

      // draw precision labels on points
      for (unsigned int point = 0; point < gae->GetN(); point++) {
        double x, y;
        gae->GetPoint(point, x, y);

        bool negative_x(false), negative_y(false);

        double errorx = gae->GetErrorXhigh(point);
        if (errorx < gae->GetErrorXlow(point)) {
          errorx = gae->GetErrorXlow(point);
          negative_x = true;
        }
        double errory = gae->GetErrorYhigh(point);
        if (errory < gae->GetErrorYlow(point)) {
          errory = gae->GetErrorYlow(point);
          negative_y = true;
        }

        std::stringstream ss;
        NeatPlotting::TextStyle text_style;
        text_style.text_size = 0.02;
        ss << std::fixed << std::setprecision(0) << 1000.0 * errorx / scales[i];
        NeatPlotting::PlotLabel xlabel(ss.str(), text_style);
        xlabel.setAbsolutionPosition(x - errorx - 0.1, y + 0.2);
        plot_bundle.plot_decoration.labels.push_back(xlabel);

        ss.str("");
        ss << std::fixed << std::setprecision(0) << 1000.0 * errory / scales[i];
        NeatPlotting::PlotLabel ylabel(ss.str(), text_style);
        double y_offset = 0.2 + errory;
        if (negative_y)
          y_offset = -0.4 - errory;
        ylabel.setAbsolutionPosition(x - 0.1, y + y_offset);
        plot_bundle.plot_decoration.labels.push_back(ylabel);

        /*std::stringstream ss;
         NeatPlotting::TextStyle text_style;
         text_style.text_size = 0.02;
         ss << std::fixed << std::setprecision(0) << "("
         << 10000.0 * errorx / scales[i] << "," << 10000.0 * errory / scales[i]
         << ")#mum";
         NeatPlotting::PlotLabel xlabel(ss.str(), text_style);
         xlabel.setAbsolutionPosition(x - 0.08, y + 0.02);
         plot_bundle.plot_decoration.labels.push_back(xlabel);*/

      }
      std::stringstream ss;
      ss << "diff scaled #times" << scales[i];
      NeatPlotting::TextStyle text_style;
      text_style.text_size = 0.04;
      NeatPlotting::PlotLabel label(ss.str(), text_style);
      label.setRelativePosition(0.60, 0.77);
      plot_bundle.plot_decoration.labels.push_back(label);
      label.setTitle("units #mum");
      label.setRelativePosition(0.60, 0.70);
      plot_bundle.plot_decoration.labels.push_back(label);

    }

    plot_bundle.plot_axis.x_axis_title = "x_{IP} / mm";
    plot_bundle.plot_axis.y_axis_title = "y_{IP} / mm";

    return plot_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeXYOverviewHistogram(
      const std::vector<PndLmdElasticDataBundle> &elastic_data_bundles) const {

    NeatPlotting::PlotBundle plot_bundle;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "COL";

    NeatPlotting::TextStyle text_style;
    text_style.text_size = 0.03;

// determine boundaries
    double x_max = 0.0, y_max = 0.0;

    std::vector<PndLmdElasticDataBundle>::const_iterator ip_setting_case;
    for (ip_setting_case = elastic_data_bundles.begin();
        ip_setting_case != elastic_data_bundles.end(); ip_setting_case++) {

      boost::property_tree::ptree sim_params(
          ip_setting_case->getSimulationParametersPropertyTree());

      if (fabs(sim_params.get<double>("ip_mean_x")) > x_max)
        x_max = 10.0 * fabs(sim_params.get<double>("ip_mean_x")); // *10 for cm to mm

      if (fabs(sim_params.get<double>("ip_mean_y")) > y_max)
        y_max = 10.0 * fabs(sim_params.get<double>("ip_mean_y"));
    }

    TH2D *hist = new TH2D("ip_shift_xy_lumi_results", "", 51, -1.1 * x_max,
        1.1 * x_max, 51, -1.1 * y_max, 1.1 * y_max);

    std::pair<double, double> lumi;

    for (ip_setting_case = elastic_data_bundles.begin();
        ip_setting_case != elastic_data_bundles.end(); ip_setting_case++) {
      if (ip_setting_case->getFitResults().size() > 0) {
        PndLmdLumiFitResult fit_result;
        fit_result.setModelFitResult(
            ip_setting_case->getFitResults().begin()->second);

        lumi = calulateRelDiff(fit_result.getLuminosity(),
            fit_result.getLuminosityError(),
            ip_setting_case->getReferenceLuminosity());

        boost::property_tree::ptree sim_params(
            ip_setting_case->getSimulationParametersPropertyTree());

        double ip_mean_x = sim_params.get<double>("ip_mean_x");
        double ip_mean_y = sim_params.get<double>("ip_mean_y");

        hist->Fill(10.0 * ip_mean_x, 10.0 * ip_mean_y, fabs(lumi.first));

        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << lumi.first << " %"; // << "#pm" << lumi.second << " %";
        NeatPlotting::PlotLabel label(ss.str(), text_style);
        label.setAbsolutionPosition(10.0 * (ip_mean_x - 0.035),
            10.0 * (ip_mean_y + 0.02));
        plot_bundle.plot_decoration.labels.push_back(label);

      }
    }

//style.line_style.line_color = kRed;
//style.marker_style.marker_color = kRed;
//style.marker_style.marker_style = 20;

    plot_bundle.addHistogram(hist, style);

    std::stringstream ss;
    ss << "stat. err.: " << std::fixed << std::setprecision(2) << lumi.second
        << " %";
    NeatPlotting::PlotLabel label(ss.str(), text_style);
    label.setRelativePosition(0.6, 0.8);
    plot_bundle.plot_decoration.labels.push_back(label);

    plot_bundle.plot_axis.x_axis_title = "x_{IP} / mm";
    plot_bundle.plot_axis.y_axis_title = "y_{IP} / mm";
    plot_bundle.plot_axis.z_axis_title = "#frac{|L-L_{ref}|}{L_{ref}} / %";

    return plot_bundle;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::makeTiltXYOverviewHistogram(
      const std::vector<PndLmdElasticDataBundle> &elastic_data_bundles,
      int draw_labels) const {
    gStyle->SetPadRightMargin(0.21);

    NeatPlotting::PlotBundle plot_bundle;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "COL";

    NeatPlotting::TextStyle text_style;
    text_style.text_size = 0.03;

// determine boundaries
    double x_max = 0.0, y_max = 0.0;

    double unit_factor(1000.0);

    std::vector<PndLmdElasticDataBundle>::const_iterator ip_setting_case;
    for (ip_setting_case = elastic_data_bundles.begin();
        ip_setting_case != elastic_data_bundles.end(); ip_setting_case++) {

      boost::property_tree::ptree sim_params(
          ip_setting_case->getSimulationParametersPropertyTree());

      if (unit_factor * fabs(sim_params.get<double>("beam_tilt_x")) > x_max)
        x_max = unit_factor * fabs(sim_params.get<double>("beam_tilt_x")); // *1000 for rad to mrad

      if (unit_factor * fabs(sim_params.get<double>("beam_tilt_y")) > y_max)
        y_max = unit_factor * fabs(sim_params.get<double>("beam_tilt_y"));
    }

    TH2D *hist = new TH2D("beam_tilt_xy_lumi_results", "", 51, -1.1 * x_max,
        1.1 * x_max, 51, -1.1 * y_max, 1.1 * y_max);

    std::pair<double, double> lumi;

    text_style.text_size = 0.026;

    for (ip_setting_case = elastic_data_bundles.begin();
        ip_setting_case != elastic_data_bundles.end(); ip_setting_case++) {
      if (ip_setting_case->getFitResults().size() > 0) {
        PndLmdLumiFitResult fit_result;
        fit_result.setModelFitResult(
            ip_setting_case->getFitResults().begin()->second);

        lumi = calulateRelDiff(fit_result.getLuminosity(),
            fit_result.getLuminosityError(),
            ip_setting_case->getReferenceLuminosity());

        boost::property_tree::ptree sim_params(
            ip_setting_case->getSimulationParametersPropertyTree());

        double beam_tilt_x = sim_params.get<double>("beam_tilt_x");
        double beam_tilt_y = sim_params.get<double>("beam_tilt_y");

        hist->Fill(unit_factor * beam_tilt_x, unit_factor * beam_tilt_y,
            fabs(lumi.first));

        if (draw_labels > 0) {
          std::stringstream ss;
          ss << std::fixed << std::setprecision(2) << lumi.first; // << "#pm" << lumi.second << " %";
          NeatPlotting::PlotLabel label(ss.str(), text_style);
          label.setAbsolutionPosition(unit_factor * beam_tilt_x - x_max * 0.05,
              unit_factor * beam_tilt_y + y_max * 0.03);
          plot_bundle.plot_decoration.labels.push_back(label);

          if (draw_labels > 1) {
            try {
              double value = fit_result.getModelFitResult().getFitParameter(
                  "tilt_x").value;
              double error = fit_result.getModelFitResult().getFitParameter(
                  "tilt_x").error;
              ss.str("");

              std::pair<double, double> reldiff = calulateRelDiff(
                  unit_factor * value, unit_factor * error,
                  unit_factor * beam_tilt_x);

              ss << std::fixed << std::setprecision(2) << "tilt_{x}="
                  << 1e3 * unit_factor * value << " #pm "
                  << 1e3 * unit_factor * error << " #murad";

              NeatPlotting::PlotLabel label(ss.str(), text_style);
              label.setAbsolutionPosition(
                  unit_factor * beam_tilt_x - x_max * 0.1,
                  unit_factor * beam_tilt_y + y_max * 0.2);
              plot_bundle.plot_decoration.labels.push_back(label);

            } catch (int n) {
              std::cout
                  << "tilt x parameter does not exist as a fit parameter in the fit result!"
                  << std::endl;
            }
            try {
              double value = fit_result.getModelFitResult().getFitParameter(
                  "tilt_y").value;
              double error = fit_result.getModelFitResult().getFitParameter(
                  "tilt_y").error;
              ss.str("");

              std::pair<double, double> reldiff = calulateRelDiff(
                  unit_factor * value, unit_factor * error,
                  unit_factor * beam_tilt_y);

              ss << std::fixed << std::setprecision(2) << "tilt_{y}="
                  << 1e3 * unit_factor * value << " #pm "
                  << 1e3 * unit_factor * error << " #murad";

              NeatPlotting::PlotLabel label(ss.str(), text_style);
              label.setAbsolutionPosition(
                  unit_factor * beam_tilt_x - x_max * 0.1,
                  unit_factor * beam_tilt_y + y_max * 0.13);
              plot_bundle.plot_decoration.labels.push_back(label);
            } catch (int n) {
              std::cout
                  << "tilt y parameter does not exist as a fit parameter in the fit result!"
                  << std::endl;
            }
          }
        }
      }
    }

    plot_bundle.addHistogram(hist, style);

    if (draw_labels > 0) {
      std::stringstream ss;
      ss << "stat. err.: " << std::fixed << std::setprecision(2) << lumi.second
          << " %";
      NeatPlotting::PlotLabel label(ss.str(), text_style);
      label.setRelativePosition(0.6, 1.05);
      plot_bundle.plot_decoration.labels.push_back(label);
    }

    plot_bundle.plot_axis.x_axis_title = "tilt_{x} / mrad";
    plot_bundle.plot_axis.y_axis_title = "tilt_{y} / mrad";
    plot_bundle.plot_axis.z_axis_title = "#frac{|L-L_{ref}|}{L_{ref}} / %";

    return plot_bundle;
  }

// booky creation

  bool PndLmdPlotter::compareLumiModelOptions(
      const boost::property_tree::ptree &pt,
      const LumiFit::LmdDimensionOptions &dim_opt) const {
// just convert the dim_opt to a ptree and then call the ptree comparator...
    boost::property_tree::ptree rhs_pt;

    bool acc_active(false);
    bool res_active(false);
    if (dim_opt.track_type == LumiFit::MC_ACC)
      acc_active = true;
    if (dim_opt.track_type == LumiFit::RECO) {
      acc_active = true;
      res_active = true;
    }

    rhs_pt.put("acceptance_correction_active", acc_active);
    rhs_pt.put("resolution_smearing_active", res_active);

    return compareLumiModelOptions(pt, rhs_pt);
  }

  bool PndLmdPlotter::compareLumiModelOptions(
      const boost::property_tree::ptree &pt_lhs,
      const boost::property_tree::ptree &pt_rhs) const {

    bool lhs_acc_corr_active = pt_lhs.get<bool>("acceptance_correction_active");
    bool lhs_res_smear_active = pt_lhs.get<bool>("resolution_smearing_active");

    bool rhs_acc_corr_active = pt_rhs.get<bool>("acceptance_correction_active");
    bool rhs_res_smear_active = pt_rhs.get<bool>("resolution_smearing_active");

    return (lhs_acc_corr_active == rhs_acc_corr_active
        && lhs_res_smear_active == rhs_res_smear_active);
  }

  NeatPlotting::Booky PndLmdPlotter::makeLumiFitResultOverviewBooky(
      std::vector<PndLmdAngularData> &data_vec) {

    std::map<PndLmdFitOptions,
        std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>,
        LumiFit::Comparisons::FitOptionsCompare> ordered_plots;

    NeatPlotting::PlotStyle log_y_plot_style;
    log_y_plot_style.y_axis_style.log_scale = true;
    NeatPlotting::PlotStyle normal_plot_style;

// model types
    LumiFit::LmdDimensionOptions fitop_tmctruth;
    fitop_tmctruth.dimension_type = LumiFit::T;
    fitop_tmctruth.track_type = LumiFit::MC;
    LumiFit::LmdDimensionOptions fitop_thmctruth;
    fitop_thmctruth.dimension_type = LumiFit::THETA;
    fitop_thmctruth.track_type = LumiFit::MC;
    LumiFit::LmdDimensionOptions fitop_mcacc;
    fitop_mcacc.dimension_type = LumiFit::THETA;
    fitop_mcacc.track_type = LumiFit::MC_ACC;
    LumiFit::LmdDimensionOptions fitop_normal;
    fitop_normal.dimension_type = LumiFit::THETA;
    fitop_normal.track_type = LumiFit::RECO;

// go through data map
    for (std::vector<PndLmdAngularData>::iterator top_it = data_vec.begin();
        top_it != data_vec.end(); top_it++) {

      // we only want IP info, so throw out everything else
      if (top_it->getPrimaryDimension().dimension_options.track_param_type
          != LumiFit::IP) {
        continue;
      }
      // ok we are actually only interested in the dimension type and track type

      // loop over fit results
      map<PndLmdFitOptions, ModelFitResult> fit_results =
          top_it->getFitResults();
      for (map<PndLmdFitOptions, ModelFitResult>::iterator it =
          fit_results.begin(); it != fit_results.end(); it++) {

        // PAD1: if we have MC data and momentum transfer and we fit that with mc_t model
        // skip this pad in the beginning to find the matching panels later due to different fit ranges

        // mc stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::MC
            && top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA) {
          NeatPlotting::SubpadCoordinates pad_coord(2, 1);

          if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
              fitop_thmctruth)) {
            ordered_plots[it->first][pad_coord] = makeGraphBundle(*top_it,
                it->first);
          }
        }

        // mc acc stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::MC_ACC
            && top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA) {
          NeatPlotting::SubpadCoordinates pad_coord(3, 1);

          if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
              fitop_mcacc)) {
            //ordered_plots[it->first][pad_coord] = makeAcceptanceBundle1D(*top_it,
            //		it->first);

            pad_coord = std::make_pair(1, 2);

            ordered_plots[it->first][pad_coord] = makeGraphBundle(*top_it,
                it->first);

            // create residual beneath this plot
            pad_coord.second = 3;

            ordered_plots[it->first][pad_coord] = makeResidualPlotBundle1D(
                *top_it, it->first);
          }

        }
        // reco stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::RECO
            && top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA) {
          bool with_secondaries(true);

          const std::set<LumiFit::LmdDimension> &selection_set =
              top_it->getSelectorSet();

          std::set<LumiFit::LmdDimension>::const_iterator selection_set_it;
          for (selection_set_it = selection_set.begin();
              selection_set_it != selection_set.end(); selection_set_it++) {
            if (selection_set_it->dimension_options.dimension_type
                == LumiFit::SECONDARY) {
              if (selection_set_it->dimension_range.getDimensionMean() < 0) {
                with_secondaries = false;
                break;
              }
            }
          }

          if (with_secondaries) {
            NeatPlotting::SubpadCoordinates pad_coord(3, 2);

            if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
                fitop_normal)) {
              ordered_plots[it->first][pad_coord] = makeGraphBundle(*top_it,
                  it->first);

              // create residual beneath this plot
              pad_coord.second = 3;

              ordered_plots[it->first][pad_coord] = makeResidualPlotBundle1D(
                  *top_it, it->first);
            }
          } else {
            NeatPlotting::SubpadCoordinates pad_coord(2, 2);

            if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
                fitop_normal)) {
              ordered_plots[it->first][pad_coord] = makeGraphBundle(*top_it,
                  it->first);

              // create residual beneath this plot
              pad_coord.second = 3;

              ordered_plots[it->first][pad_coord] = makeResidualPlotBundle1D(
                  *top_it, it->first);
            }
          }
        }
      }
    }

    /*
     // go through data map again and try to match the momentum transfer data fits
     for (std::vector<PndLmdAngularData>::iterator top_it = data_vec.begin();
     top_it != data_vec.end(); top_it++) {

     // first determine which data objects we have here
     if (top_it->getPrimaryDimension().dimension_options.track_param_type
     != LumiFit::IP) {
     break;
     }
     // ok we are actually only interested in the dimension type and track type

     // PAD1: if we have MC data and momentum transfer and we fit that with mc_t model
     if (top_it->getPrimaryDimension().dimension_options.track_type
     == LumiFit::MC
     && top_it->getPrimaryDimension().dimension_options.dimension_type
     == LumiFit::T) {
     int pad_number = 1;
     map<PndLmdLumiFitOptions, PndLmdLumiFitResult*> fit_results =
     top_it->getFitResults();
     for (map<PndLmdLumiFitOptions, PndLmdLumiFitResult*>::iterator it =
     fit_results.begin(); it != fit_results.end(); it++) {
     if (fitop_tmctruth.equalBinaryOptions(it->first.getFitModelOptions())) {

     // loop over current return map and find appropriate canvas
     for (std::map<PndLmdLumiFitOptions,
     std::map<int, NeatPlotting::PlotBundle>,
     LumiFit::Comparisons::fit_options_compare>::iterator return_map_it =
     ordered_plots.begin(); return_map_it != ordered_plots.end();
     return_map_it++) {
     DataStructs::DimensionRange lmd_range =
     return_map_it->first.getEstimatorOptions().getFitRangeX();

     if (it->first.getEstimatorOptions().getFitRangeX().range_low
     == lumi_helper.getMomentumTransferFromTheta(
     top_it->getLabMomentum(), lmd_range.range_low)) {
     if (it->first.getEstimatorOptions().getFitRangeX().range_high
     == lumi_helper.getMomentumTransferFromTheta(
     top_it->getLabMomentum(), lmd_range.range_high)) {
     return_map_it->second[pad_number] = makeGraphBundle1D(*top_it,
     it->first);
     break;
     }
     }
     }
     }
     }
     }
     }*/

    NeatPlotting::Booky booky;

    for (std::map<PndLmdFitOptions,
        std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>,
        LumiFit::Comparisons::FitOptionsCompare>::iterator data_for_booky_page_it =
        ordered_plots.begin(); data_for_booky_page_it != ordered_plots.end();
        data_for_booky_page_it++) {

      std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>::iterator single_pad_plot_bundle_it;
      for (single_pad_plot_bundle_it = data_for_booky_page_it->second.begin();
          single_pad_plot_bundle_it != data_for_booky_page_it->second.end();
          single_pad_plot_bundle_it++) {
        booky.addPlotToCurrentBookyPage(single_pad_plot_bundle_it->second,
            normal_plot_style, single_pad_plot_bundle_it->first);
      }
      booky.addCurrentPageToBooky();
    }

    return booky;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::createLumiFit2DPlotBundle(
      TH2D * hist) const {
    NeatPlotting::PlotBundle ang_plot_bundle;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "COL";

    ang_plot_bundle.addHistogram(hist, style);

    std::stringstream ss;

    applyPlotRanges(ang_plot_bundle);
    ang_plot_bundle.plot_axis.x_axis_title = "#theta";
    ss << "# of tracks / " << 1000.0 * hist->GetXaxis()->GetBinWidth(1)
        << " mrad";

    ang_plot_bundle.plot_axis.y_axis_title = ss.str();

    return ang_plot_bundle;
  }

  NeatPlotting::Booky PndLmdPlotter::create2DFitResultPlots(
      const PndLmdFitDataBundle &fit_data_bundle) {

    std::map<PndLmdFitOptions,
        std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>,
        LumiFit::Comparisons::FitOptionsCompare> ordered_plots;

    NeatPlotting::PlotStyle normal_plot_style;
    normal_plot_style.palette_color_style = 1;

// model types
    LumiFit::LmdDimensionOptions fitop_thmctruth;
    fitop_thmctruth.dimension_type = LumiFit::THETA;
    fitop_thmctruth.track_type = LumiFit::MC;
    LumiFit::LmdDimensionOptions fitop_mcacc;
    fitop_mcacc.dimension_type = LumiFit::THETA;
    fitop_mcacc.track_type = LumiFit::MC_ACC;
    LumiFit::LmdDimensionOptions fitop_normal;
    fitop_normal.dimension_type = LumiFit::THETA;
    fitop_normal.track_type = LumiFit::RECO;

    current_fit_bundle = fit_data_bundle;

// go through data map
    for (std::vector<PndLmdElasticDataBundle>::const_iterator top_it =
        fit_data_bundle.getElasticDataBundles().begin();
        top_it != fit_data_bundle.getElasticDataBundles().end(); ++top_it) {

      // we only want IP info, so throw out everything else
      if (top_it->getPrimaryDimension().dimension_options.track_param_type
          != LumiFit::IP) {
        continue;
      }
      // ok we are actually only interested in the dimension type and track type
      // loop over fit results
      map<PndLmdFitOptions, ModelFitResult> fit_results =
          top_it->getFitResults();
      for (map<PndLmdFitOptions, ModelFitResult>::iterator it =
          fit_results.begin(); it != fit_results.end(); it++) {

        // set the appropriate acceptance and resolutions in the factory
        // in principle there could be multiple acceptances used here but we assume there is just 1
        /*if (top_it->getUsedAcceptanceIndices().size() > 0) {
         unsigned int used_acceptance_index =
         top_it->getUsedAcceptanceIndices()[0];
         const PndLmdAcceptance &used_acceptance =
         fit_data_bundle.getUsedAcceptancesPool()[used_acceptance_index];
         lmd_fit_facade.setModelFactoryAcceptence(used_acceptance);
         }
         if (top_it->getUsedResolutionsIndexRanges().size() > 0) {
         std::vector<PndLmdHistogramData> used_resolutions;
         for (unsigned int res_index_range = 0;
         res_index_range < top_it->getUsedResolutionsIndexRanges().size();
         ++res_index_range) {
         used_resolutions.insert(used_resolutions.end(),
         fit_data_bundle.getUsedResolutionsPool().begin()
         + top_it->getUsedResolutionsIndexRanges()[res_index_range].first,
         fit_data_bundle.getUsedResolutionsPool().begin()
         + top_it->getUsedResolutionsIndexRanges()[res_index_range].second);
         }
         lmd_fit_facade.setModelFactoryResolutions(used_resolutions);
         }*/

        // PAD1: if we have MC data and momentum transfer and we fit that with mc_t model
        // skip this pad in the beginning to find the matching panels later due to different fit ranges
        // mc stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::MC
            && (top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA_X
                || top_it->getPrimaryDimension().dimension_options.dimension_type
                    == LumiFit::THETA)) {

          if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
              fitop_thmctruth)) {
            NeatPlotting::SubpadCoordinates pad_coord(1, 1);
            TH2D *data = top_it->get2DHistogram();
            ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                data);

            ModelFitResult fit_res = top_it->getFitResult(it->first);
            if (0 == fit_res.getFitStatus()) {
              TH2D* model = create2DHistogramFromFitResult(it->first, *top_it);
              pad_coord = std::make_pair(1, 2);
              ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                  model);
              TH2D* diff = neat_plot_helper.makeDifferenceHistogram(data,
                  model);
              pad_coord = std::make_pair(1, 3);
              ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                  diff);

              double lumi_ref = top_it->getReferenceLuminosity();

              stringstream strstream;
              strstream.precision(3);

              strstream << "p_{lab} = " << top_it->getLabMomentum() << " GeV";

              NeatPlotting::TextStyle text_style;
              NeatPlotting::PlotLabel plot_label(strstream.str(), text_style);
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_leftpos =
                  0.7;
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_toppos =
                  0.95;
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_spacing =
                  0.06;
              ordered_plots[it->first][pad_coord].plot_decoration.use_line_layout =
                  true;
              ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                  plot_label);
              strstream.str("");

              if (fit_res.getFitStatus() == 0) {
                strstream.str("");
                strstream << std::setprecision(3) << "#Delta L/L = "
                    << calulateRelDiff(getLuminosity(fit_res),
                        getLuminosityError(fit_res), lumi_ref).first << " #pm "
                    << calulateRelDiff(getLuminosity(fit_res),
                        getLuminosityError(fit_res), lumi_ref).second << " %";
                plot_label.setTitle(strstream.str());
                ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                    plot_label);

                const std::set<ModelStructs::minimization_parameter> fit_params =
                    fit_res.getFitParameters();
                std::set<ModelStructs::minimization_parameter>::const_iterator fit_param_it;
                for (fit_param_it = fit_params.begin();
                    fit_param_it != fit_params.end(); fit_param_it++) {
                  strstream.str("");
                  strstream << std::setprecision(3) << fit_param_it->name.second
                      << "= " << fit_param_it->value << " #pm "
                      << fit_param_it->error;
                  plot_label.setTitle(strstream.str());

                  ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                      plot_label);
                }
              }
            }
          }
        }

        // mc acc stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::MC_ACC
            && (top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA_X
                || top_it->getPrimaryDimension().dimension_options.dimension_type
                    == LumiFit::THETA)) {

          if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
              fitop_mcacc)) {
            NeatPlotting::SubpadCoordinates pad_coord(2, 1);
            TH2D *data = top_it->get2DHistogram();
            ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                data);

            ModelFitResult fit_res = top_it->getFitResult(it->first);
            if (0 == fit_res.getFitStatus()) {
              TH2D* model = create2DHistogramFromFitResult(it->first, *top_it);
              pad_coord = std::make_pair(2, 2);
              ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                  model);
              TH2D* diff = neat_plot_helper.makeDifferenceHistogram(data,
                  model);
              pad_coord = std::make_pair(2, 3);
              ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                  diff);

              double lumi_ref = top_it->getReferenceLuminosity();

              stringstream strstream;
              strstream.precision(3);

              strstream << "p_{lab} = " << top_it->getLabMomentum() << " GeV";

              NeatPlotting::TextStyle text_style;
              NeatPlotting::PlotLabel plot_label(strstream.str(), text_style);
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_leftpos =
                  0.7;
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_toppos =
                  0.95;
              ordered_plots[it->first][pad_coord].plot_decoration.label_text_spacing =
                  0.06;
              ordered_plots[it->first][pad_coord].plot_decoration.use_line_layout =
                  true;
              ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                  plot_label);
              strstream.str("");

              if (fit_res.getFitStatus() == 0) {
                strstream.str("");
                strstream << std::setprecision(3) << "#Delta L/L = "
                    << calulateRelDiff(getLuminosity(fit_res),
                        getLuminosityError(fit_res), lumi_ref).first << " #pm "
                    << calulateRelDiff(getLuminosity(fit_res),
                        getLuminosityError(fit_res), lumi_ref).second << " %";
                plot_label.setTitle(strstream.str());
                ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                    plot_label);

                const std::set<ModelStructs::minimization_parameter> fit_params =
                    fit_res.getFitParameters();
                std::set<ModelStructs::minimization_parameter>::const_iterator fit_param_it;
                for (fit_param_it = fit_params.begin();
                    fit_param_it != fit_params.end(); fit_param_it++) {
                  strstream.str("");
                  strstream << std::setprecision(3) << fit_param_it->name.second
                      << "= " << fit_param_it->value << " #pm "
                      << fit_param_it->error;
                  plot_label.setTitle(strstream.str());

                  ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                      plot_label);
                }
              }
            }
          }

        }
        // reco stuff:
        if (top_it->getPrimaryDimension().dimension_options.track_type
            == LumiFit::RECO
            && (top_it->getPrimaryDimension().dimension_options.dimension_type
                == LumiFit::THETA_X
                || top_it->getPrimaryDimension().dimension_options.dimension_type
                    == LumiFit::THETA)) {
          bool with_secondaries(true);

          const std::set<LumiFit::LmdDimension> &selection_set =
              top_it->getSelectorSet();

          std::set<LumiFit::LmdDimension>::const_iterator selection_set_it;
          for (selection_set_it = selection_set.begin();
              selection_set_it != selection_set.end(); selection_set_it++) {
            if (selection_set_it->dimension_options.dimension_type
                == LumiFit::SECONDARY) {
              if (selection_set_it->dimension_range.getDimensionMean() < 0) {
                with_secondaries = false;
                break;
              }
            }
          }

          if (with_secondaries) {
            if (compareLumiModelOptions(it->first.getModelOptionsPropertyTree(),
                fitop_normal)) {
              NeatPlotting::SubpadCoordinates pad_coord(3, 1);
              TH2D *data = top_it->get2DHistogram();
              ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                  data);

              ModelFitResult fit_res = top_it->getFitResult(it->first);
              if (0 == fit_res.getFitStatus()) {
                TH2D* model = create2DHistogramFromFitResult(it->first,
                    *top_it);
                pad_coord = std::make_pair(3, 2);
                ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                    model);

                TH2D* diff = neat_plot_helper.makeDifferenceHistogram(data,
                    model);
                pad_coord = std::make_pair(3, 3);
                ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
                    diff);

                double lumi_ref = top_it->getReferenceLuminosity();

                stringstream strstream;
                strstream.precision(3);

                strstream << "p_{lab} = " << top_it->getLabMomentum() << " GeV";

                NeatPlotting::TextStyle text_style;
                NeatPlotting::PlotLabel plot_label(strstream.str(), text_style);
                ordered_plots[it->first][pad_coord].plot_decoration.label_text_leftpos =
                    0.7;
                ordered_plots[it->first][pad_coord].plot_decoration.label_text_toppos =
                    0.95;
                ordered_plots[it->first][pad_coord].plot_decoration.label_text_spacing =
                    0.06;
                ordered_plots[it->first][pad_coord].plot_decoration.use_line_layout =
                    true;
                ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                    plot_label);
                strstream.str("");

                if (fit_res.getFitStatus() == 0) {
                  strstream.str("");
                  strstream << std::setprecision(3) << "#Delta L/L = "
                      << calulateRelDiff(getLuminosity(fit_res),
                          getLuminosityError(fit_res), lumi_ref).first
                      << " #pm "
                      << calulateRelDiff(getLuminosity(fit_res),
                          getLuminosityError(fit_res), lumi_ref).second << " %";
                  plot_label.setTitle(strstream.str());
                  ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                      plot_label);

                  const std::set<ModelStructs::minimization_parameter> fit_params =
                      fit_res.getFitParameters();
                  std::set<ModelStructs::minimization_parameter>::const_iterator fit_param_it;
                  for (fit_param_it = fit_params.begin();
                      fit_param_it != fit_params.end(); fit_param_it++) {
                    strstream.str("");
                    strstream << std::setprecision(3)
                        << fit_param_it->name.second << "= "
                        << fit_param_it->value << " #pm "
                        << fit_param_it->error;
                    plot_label.setTitle(strstream.str());

                    ordered_plots[it->first][pad_coord].plot_decoration.labels.push_back(
                        plot_label);
                  }
                }
              }
            }
          } else {
            /*NeatPlotting::SubpadCoordinates pad_coord(2, 2);

             if (fitop_normal.equalBinaryOptions(it->first.getFitModelOptions())) {
             NeatPlotting::SubpadCoordinates pad_coord(1, 1);
             TH2D *data = top_it->get2DHistogram();
             ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
             data);

             PndLmdLumiFitResult *fit_res = top_it->getFitResult(it->first);
             if (0 == fit_res->getModelFitResult().getFitStatus()) {
             TH2D* model = create2DHistogramFromFitResult(it->first, *top_it);
             NeatPlotting::SubpadCoordinates pad_coord(1, 2);
             ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
             model);
             TH2D* diff = neat_plot_helper.makeDifferenceHistogram(data,
             model);
             NeatPlotting::SubpadCoordinates pad_coord(1, 3);
             ordered_plots[it->first][pad_coord] = createLumiFit2DPlotBundle(
             diff);
             }*/
          }
        }
      }
    }

    NeatPlotting::Booky booky;

    for (std::map<PndLmdFitOptions,
        std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>,
        LumiFit::Comparisons::FitOptionsCompare>::iterator data_for_booky_page_it =
        ordered_plots.begin(); data_for_booky_page_it != ordered_plots.end();
        data_for_booky_page_it++) {

      std::map<NeatPlotting::SubpadCoordinates, NeatPlotting::PlotBundle>::iterator single_pad_plot_bundle_it;
      for (single_pad_plot_bundle_it = data_for_booky_page_it->second.begin();
          single_pad_plot_bundle_it != data_for_booky_page_it->second.end();
          single_pad_plot_bundle_it++) {
        booky.addPlotToCurrentBookyPage(single_pad_plot_bundle_it->second,
            normal_plot_style, single_pad_plot_bundle_it->first);
      }
      booky.addCurrentPageToBooky();
    }

    return booky;

  }

// here only a specific ip setting and dimensioning should be used
  NeatPlotting::Booky PndLmdPlotter::makeVertexFitResultBooky(
      const std::vector<PndLmdHistogramData> &data_vec) {

    NeatPlotting::Booky booky;

    NeatPlotting::PlotStyle plot_style;

    for (unsigned int i = 0; i < data_vec.size(); i++) {
      NeatPlotting::SubpadCoordinates pad_coord;

      NeatPlotting::PlotBundle plot_bundle(
          makeVertexGraphBundle1D(data_vec[i]));

      if (data_vec[i].getName().find("mc_x") != std::string::npos) {
        pad_coord = std::make_pair(1, 1);
        plot_style.y_axis_style.log_scale = true;
      } else if (data_vec[i].getName().find("mc_y") != std::string::npos) {
        pad_coord = std::make_pair(2, 1);
        plot_style.y_axis_style.log_scale = true;
      } else if (data_vec[i].getName().find("mc_z") != std::string::npos) {
        pad_coord = std::make_pair(3, 1);
        plot_style.y_axis_style.log_scale = true;
      } else if (data_vec[i].getName().find("reco_x") != std::string::npos) {
        pad_coord = std::make_pair(1, 2);
        plot_style.y_axis_style.log_scale = false;
      } else if (data_vec[i].getName().find("reco_y") != std::string::npos) {
        pad_coord = std::make_pair(2, 2);
        plot_style.y_axis_style.log_scale = false;
      } else if (data_vec[i].getName().find("reco_z") != std::string::npos) {
        pad_coord = std::make_pair(3, 2);
        plot_style.y_axis_style.log_scale = false;
      }

      booky.addPlotToCurrentBookyPage(plot_bundle, plot_style, pad_coord);
    }
    booky.addCurrentPageToBooky();
    return booky;
  }

  void PndLmdPlotter::makeVertexDifferencesBooky(
      std::vector<PndLmdHistogramData> &res_vec,
      std::vector<PndLmdHistogramData> &res_vec_ref) {
  }

  void PndLmdPlotter::createAcceptanceComparisonBooky(
      std::vector<std::pair<PndLmdAcceptance, PndLmdAcceptance> > &acc_matches) {

    NeatPlotting::Booky booky;

    NeatPlotting::DataObjectStyle style;
    style.draw_option = "PE";
    style.marker_style.marker_style = 20;

    NeatPlotting::DataObjectStyle style2;
    style2.draw_option = "COL";

    NeatPlotting::PlotStyle plot_style;
    plot_style.palette_color_style = 1;

// create new page for each matched pair
    for (unsigned int i = 0; i < acc_matches.size(); i++) {
      std::pair<PndLmdAcceptance, PndLmdAcceptance> acc_pair = acc_matches[i];

      /*	NeatPlotting::PlotBundle plot_bundle;
       style.line_style.line_color = kRed;
       style.marker_style.marker_color = kRed;

       TGraphAsymmErrors *acc = createAcceptanceGraph(&acc_pair.second);
       plot_bundle.addGraph(acc, style);

       plot_bundle.plot_axis.x_axis_title =
       acc_pair.first.getPrimaryDimension().createAxisLabel();
       plot_bundle.plot_axis.y_axis_title = "efficiency";

       style.line_style.line_color = kBlack;
       style.marker_style.marker_color = kBlack;
       TGraphAsymmErrors *ref_acc = createAcceptanceGraph(&acc_pair.first);
       plot_bundle.addGraph(ref_acc, style);

       // add full efficiency line
       plot_bundle.plot_decoration.x_parallel_lines.push_back(1.0);

       booky.addPlotToCurrentBookyPage(plot_bundle, plot_style,
       NeatPlotting::SubpadCoordinates(1, 1));

       // ---------- ratio ----------
       NeatPlotting::PlotBundle ratio_bundle;
       ratio_bundle.plot_axis.y_axis_range.active = true;
       ratio_bundle.plot_axis.y_axis_range.low = 0.9;
       ratio_bundle.plot_axis.y_axis_range.high = 1.1;

       style.line_style.line_color = kRed;
       style.marker_style.marker_color = kRed;

       TGraphAsymmErrors *ratio = neat_plot_helper.makeRatioGraph(acc, ref_acc);
       ratio_bundle.addGraph(ratio, style);

       ratio_bundle.plot_axis.x_axis_title =
       acc_pair.first.getPrimaryDimension().createAxisLabel();
       ratio_bundle.plot_axis.y_axis_title = "efficiency ratio";

       // add full efficiency line
       ratio_bundle.plot_decoration.x_parallel_lines.push_back(1.0);

       booky.addPlotToCurrentBookyPage(ratio_bundle, plot_style,
       NeatPlotting::SubpadCoordinates(2, 1));
       */
      // ---------- 2d stuff ----------
      NeatPlotting::PlotBundle plot_bundle_a1;

      TH2D* acchist1 = createAcceptanceHistogram(acc_pair.first);
      //acchist1 = (TH2D*)acchist1->Rebin2D(4,4);
      /*	plot_bundle_a1.addHistogram(acchist1, style2);

       applyPlotRanges(plot_bundle_a1);
       booky.addPlotToCurrentBookyPage(plot_bundle_a1, plot_style,
       NeatPlotting::SubpadCoordinates(1, 1));

       NeatPlotting::PlotBundle plot_bundle_a2;*/

      TH2D* acchist2 = createAcceptanceHistogram(acc_pair.second);
      //acchist2 = (TH2D*)acchist2->Rebin2D(4,4);
      /*	plot_bundle_a2.addHistogram(acchist2, style2);
       applyPlotRanges(plot_bundle_a2);

       booky.addPlotToCurrentBookyPage(plot_bundle_a2, plot_style,
       NeatPlotting::SubpadCoordinates(2, 1));*/

      NeatPlotting::PlotBundle plot_bundle_diff;
      TH2D* acc2diff = neat_plot_helper.makeDifferenceHistogram(acchist2,
          acchist1);
      plot_bundle_diff.addHistogram(acc2diff, style2);

      booky.addPlotToCurrentBookyPage(plot_bundle_diff, plot_style,
          NeatPlotting::SubpadCoordinates(1, 1));

      NeatPlotting::PlotBundle plot_bundle_reldiff;
      TH2D* acc2reldiff = neat_plot_helper.makeRelativeDifferenceHistogram(
          acchist2, acchist1);
      plot_bundle_reldiff.addHistogram(acc2reldiff, style2);
      plot_bundle_reldiff.plot_axis.z_axis_range.active = true;
      plot_bundle_reldiff.plot_axis.z_axis_range.low = -0.02;
      plot_bundle_reldiff.plot_axis.z_axis_range.high = 0.02;

      booky.addPlotToCurrentBookyPage(plot_bundle_reldiff, plot_style,
          NeatPlotting::SubpadCoordinates(2, 1));

      booky.addCurrentPageToBooky();
    }
    booky.createBooky("acceptance_comparison_booky.pdf");
  }

// resolution booky stuff
  NeatPlotting::Booky PndLmdPlotter::makeResolutionFitResultBooky(
      const std::vector<PndLmdHistogramData> &data_vec, int x, int y) const {

    NeatPlotting::Booky booky;

    NeatPlotting::PlotStyle plot_style;
    plot_style.palette_color_style = 1;

    for (unsigned int i = 0; i < data_vec.size(); i++) {
      NeatPlotting::SubpadCoordinates pad_coord(i % x + 1,
          (i % (x * y)) / x + 1);

      NeatPlotting::PlotBundle plot_bundle = makeResolutionGraphBundle(
          data_vec[i]);

      booky.addPlotToCurrentBookyPage(plot_bundle, plot_style, pad_coord);

      // if i is multiple of x*y append page
      if ((i + 1) % (x * y) == 0 || i + 1 == data_vec.size())
        booky.addCurrentPageToBooky();
    }
    return booky;
  }

  NeatPlotting::PlotBundle PndLmdPlotter::createLowerFitRangeDependencyPlotBundle(
      const std::vector<PndLmdAngularData> &data_vec,
      const LmdDimensionOptions& dim_opt) const {
// cluster data
    std::map<PndLmdHistogramData, std::vector<PndLmdAngularData> > clustered_data =
        clusterIntoGroups(data_vec);

// now create a graph for each cluster and add to bundle
    NeatPlotting::PlotBundle plot_bundle;

    NeatPlotting::DataObjectStyle data_style;
    data_style.draw_option = "PE";

    unsigned int max_graph_number = 9;
    int colors[9] = { 1, 2, 8, 9, 6, 3, 4, 30, 38 };
    int *pcolor = &colors[0];

    std::map<PndLmdHistogramData, std::vector<PndLmdAngularData> >::const_iterator it;
    if (max_graph_number >= clustered_data.size()) {
      for (it = clustered_data.begin(); it != clustered_data.end(); it++) {
        TGraphAsymmErrors *tgraph = createLowerFitRangeDependencyGraph(
            it->second, dim_opt);

        data_style.line_style.line_color = *pcolor;
        data_style.marker_style.marker_color = *pcolor;

        plot_bundle.addGraph(tgraph, data_style);
        pcolor++;
      }
    }

    return plot_bundle;
  }

  std::map<PndLmdHistogramData, std::vector<PndLmdAngularData> > PndLmdPlotter::clusterIntoGroups(
      const std::vector<PndLmdAngularData> &data_vec) const {
    std::map<PndLmdHistogramData, std::vector<PndLmdAngularData> > clustered_data;

    for (unsigned int i = 0; i < data_vec.size(); i++) {
      clustered_data[data_vec[i]].push_back(data_vec[i]);
    }

    return clustered_data;
  }

// we assume that the filtered_data list contains only the correct data types
  TGraphAsymmErrors* PndLmdPlotter::createLowerFitRangeDependencyGraph(
      const std::vector<PndLmdAngularData> &prefiltered_data,
      const LmdDimensionOptions& dim_opt) const {

    std::vector<NeatPlotting::GraphPoint> graph_data;

    for (unsigned int i = 0; i < prefiltered_data.size(); i++) {
      double lumi_ref = prefiltered_data[i].getReferenceLuminosity();

      NeatPlotting::GraphPoint graph_point;

      const map<PndLmdFitOptions, ModelFitResult> &fit_results =
          prefiltered_data[i].getFitResults();

      for (map<PndLmdFitOptions, ModelFitResult>::const_iterator iter =
          fit_results.begin(); iter != fit_results.end(); iter++) {

        if (compareLumiModelOptions(iter->first.getModelOptionsPropertyTree(),
            dim_opt)) {
          std::pair<double, double> lumival = calulateRelDiff(
              getLuminosity(iter->second), getLuminosityError(iter->second),
              lumi_ref);
          graph_point.x =
              iter->first.getEstimatorOptions().getFitRangeX().range_low;
          graph_point.y = lumival.first;
          graph_point.y_err_low = lumival.second;
          graph_point.y_err_high = lumival.second;

          graph_data.push_back(graph_point);
        }
      }
    }

    NeatPlotting::GraphAndHistogramHelper graph_helper;
    return graph_helper.makeGraph(graph_data);
  }

  NeatPlotting::SystematicsAnalyser::SystematicDependencyGraphBundle PndLmdPlotter::createLowerFitRangeDependencyGraphBundle(
      const std::vector<PndLmdAngularData> &prefiltered_data,
      const LmdDimensionOptions& dim_opt) const {

    NeatPlotting::SystematicsAnalyser systematics_analyzer;

    for (unsigned int i = 0; i < prefiltered_data.size(); i++) {
      double lumi_ref = prefiltered_data[i].getReferenceLuminosity();

      NeatPlotting::GraphPoint graph_point;

      const map<PndLmdFitOptions, ModelFitResult> &fit_results =
          prefiltered_data[i].getFitResults();

      for (map<PndLmdFitOptions, ModelFitResult>::const_iterator iter =
          fit_results.begin(); iter != fit_results.end(); iter++) {

        if (compareLumiModelOptions(iter->first.getModelOptionsPropertyTree(),
            dim_opt)) {
          std::pair<double, double> lumival = calulateRelDiff(
              getLuminosity(iter->second), getLuminosityError(iter->second),
              lumi_ref);
          systematics_analyzer.insertDependencyValues(
              iter->first.getEstimatorOptions().getFitRangeX().range_low,
              lumival.first, lumival.second);
        }
      }
    }

    return systematics_analyzer.createSystematicsGraphBundle();
  }

  TGraphAsymmErrors* PndLmdPlotter::createBinningDependencyGraphBundle(
      const std::vector<PndLmdElasticDataBundle> &prefiltered_data,
      const LmdDimensionOptions& dim_opt) const {

    std::vector<NeatPlotting::GraphPoint> points;

    for (auto const& prefiltered_data_obj : prefiltered_data) {
      double lumi_ref = prefiltered_data_obj.getReferenceLuminosity();

      NeatPlotting::GraphPoint graph_point;

      const map<PndLmdFitOptions, ModelFitResult> &fit_results =
          prefiltered_data_obj.getFitResults();

      for (map<PndLmdFitOptions, ModelFitResult>::const_iterator iter =
          fit_results.begin(); iter != fit_results.end(); iter++) {

        if(iter->second.getFitStatus() != 0)
          continue;
        if (compareLumiModelOptions(iter->first.getModelOptionsPropertyTree(),
            dim_opt)) {
          auto fit_params = iter->second.getFitParameters();
          for(auto fit_param : fit_params) {
            std::cout<<"fit parameter: "<<fit_param.name.second<<" "<<fit_param.value<<std::endl;
          }
          std::pair<double, double> lumival = calulateRelDiff(
              getLuminosity(iter->second), getLuminosityError(iter->second),
              lumi_ref);
          graph_point.x = prefiltered_data_obj.getPrimaryDimension().bins;
          graph_point.y = lumival.first;
          graph_point.y_err_low = lumival.second;
          graph_point.y_err_high = lumival.second;
          points.push_back(graph_point);
        }
      }
    }

    return neat_plot_helper.makeGraph(points);
  }

}
